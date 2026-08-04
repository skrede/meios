#include "verbs.h"
#include "validate.h"

#include "meios/urdf/load.h"

#include "meios/model/topology.h"

#include "meios/bundle/asset_bytes.h"

#include "meios/xacro/budget.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/structural.h"

#include "meios/io/source_stack.h"
#include "meios/io/text_reader.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"
#include "meios/diagnostic/capturing_log_sink.h"

#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <string_view>

namespace meios::cli
{

void recording_sink::log(level lvl, const std::string &message)
{
    if(lvl == level::error)
        m_messages.push_back(message);
}

void recording_sink::log(level lvl, const source_location &location, const std::string &message)
{
    if(lvl == level::error)
        m_messages.push_back(to_string(location) + ": " + message);
}

int exit_code(const validation_report &report)
{
    int code = 0;
    for(const validation_finding &finding : report.findings)
        if(code == 0 || finding.code < code)
            code = finding.code;
    return code;
}

namespace
{

// The document type routes the pipeline (a plain URDF skips the expansion stage);
// this reads the source's namespace declaration, never a diagnostic, so class
// attribution stays a function of which pass raised an error.
bool declares_xacro(std::string_view bytes)
{
    return bytes.find("xmlns:xacro") != std::string_view::npos;
}

load_options probe_options(const std::vector<std::filesystem::path> &roots)
{
    load_options opts;
    opts.topology = topology_policy::skip;
    opts.materials = material_policy::skip;
    opts.on_missing = missing_asset::warn;
    opts.strict = strictness::fail;
    opts.package_roots = roots;
    return opts;
}

bool xacro_expands(std::string_view bytes, const std::filesystem::path &path,
                   const std::vector<std::filesystem::path> &roots, recording_sink &sink)
{
    eval_scope scope;
    source_stack sources = build_sources(roots, sink);
    const expansion expanded = expand(bytes, scope, sources, path, expansion_limits{}, sink);
    return expanded.ok;
}

void add(validation_report &report, int code, std::string klass, std::vector<std::string> messages)
{
    report.findings.push_back(validation_finding{ code, std::move(klass), std::move(messages) });
    std::sort(report.findings.begin(), report.findings.end(),
              [](const validation_finding &a, const validation_finding &b) { return a.code < b.code; });
}

model<double> probe_load(const std::filesystem::path &path,
                         const std::vector<std::filesystem::path> &roots, recording_sink &well_formed)
{
    capturing_log_sink capture(well_formed);
    source_stack sources = build_sources(roots, capture);
    const expected<load_result, load_error> loaded =
        load(path, probe_options(roots), sources, capture);
    if(loaded)
        return loaded->robot;
    if(!well_formed.had_error())
        well_formed.log(level::error, loaded.error().code, loaded.error().loc, loaded.error().message);
    return {};
}

// A document that cannot be read is a document that cannot be loaded, which is what class one
// already names, so the failure takes the well-formedness channel rather than a class of its own.
validation_report refused_read(const std::filesystem::path &path, const text_read_failure &failure,
                               recording_sink &well_formed)
{
    well_formed.log(level::error, diagnostic_code::cannot_open,
                    source_location{ path.string(), 0, 0 }, failure.cause,
                    "cannot read input file \"" + path.string() + "\": "
                        + detail::read_failure_reason(failure));
    validation_report report;
    add(report, 1, "malformed", well_formed.messages());
    return report;
}

validation_report assemble_report(bool xacro, bool expand_ok, const topology_result &topo,
                                  recording_sink &well_formed, recording_sink &expansion,
                                  recording_sink &topology)
{
    validation_report report;
    // Class 1 covers a document that (or whose immediate asset references) cannot be
    // loaded, not only XML malformedness: the same well-formedness sink records both
    // strictness errors and a source's containment rejection of a root-escaping ref.
    if(well_formed.had_error() && (!xacro || expand_ok))
        add(report, 1, "malformed", well_formed.messages());
    if(xacro && !expand_ok)
        add(report, 4, "unsupported-feature", expansion.messages());
    if(!topo.ok)
        add(report, 2, "connectivity", topology.messages());
    return report;
}

}

validation_report classify(const std::filesystem::path &path,
                           const std::vector<std::filesystem::path> &roots)
{
    recording_sink well_formed;
    const text_read_result bytes = read_text_file(path);
    if(!bytes)
        return refused_read(path, bytes.error(), well_formed);

    recording_sink expansion;
    recording_sink topology;
    const model<double> robot = probe_load(path, roots, well_formed);
    const bool xacro = declares_xacro(*bytes);
    const bool expand_ok = xacro ? xacro_expands(*bytes, path, roots, expansion) : true;
    const topology_result topo =
        reconstruct_topology(robot.links, robot.joints, topology, topology_policy::fail);

    return assemble_report(xacro, expand_ok, topo, well_formed, expansion, topology);
}

}
