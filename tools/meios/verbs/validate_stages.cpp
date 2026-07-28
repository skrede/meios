#include "verbs.h"
#include "validate.h"

#include "meios/urdf/load.h"

#include "meios/model/topology.h"

#include "meios/xacro/budget.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/structural.h"

#include "meios/io/source_stack.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/capturing_log_sink.h"

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
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

std::string read_file(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

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
    const std::string bytes = read_file(path);
    const bool xacro = declares_xacro(bytes);

    recording_sink well_formed;
    recording_sink expansion;
    recording_sink topology;

    capturing_log_sink capture(well_formed);
    source_stack sources = build_sources(roots, capture);
    const expected<load_result, load_error> loaded =
        load(path, probe_options(roots), sources, capture);
    model<double> robot{};
    if(loaded)
        robot = loaded->robot;
    else if(!well_formed.had_error())
        well_formed.log(level::error, loaded.error().code, loaded.error().loc, loaded.error().message);
    const bool expand_ok = xacro ? xacro_expands(bytes, path, roots, expansion) : true;
    const topology_result topo =
        reconstruct_topology(robot.links, robot.joints, topology, topology_policy::fail);

    return assemble_report(xacro, expand_ok, topo, well_formed, expansion, topology);
}

}
