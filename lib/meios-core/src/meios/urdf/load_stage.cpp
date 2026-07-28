#include "load_stage.h"
#include "urdf_detail.h"
#include "yaml_resource.h"

#include "meios/urdf/load.h"
#include "meios/urdf/urdf_reader.h"
#include "meios/urdf/parse_context.h"

#include "meios/sink/world_recorder.h"

#include "meios/io/source_stack.h"

#include "meios/xacro/budget.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/structural.h"
#include "meios/xacro/core_evaluator.h"
#include "meios/xacro/text_resource_loader.h"

#include "meios/diagnostic/claims.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"
#include "meios/diagnostic/capturing_log_sink.h"

#include <pugixml.hpp>

#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

namespace
{

std::optional<std::string> read_file(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    if(!in)
        return std::nullopt;
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

pugi::xml_node first_element(pugi::xml_node document)
{
    for(pugi::xml_node child : document.children())
        if(child.type() == pugi::node_element)
            return child;
    return {};
}

bool declares_xacro(pugi::xml_node root)
{
    for(pugi::xml_attribute attr : root.attributes())
        if(std::string_view(attr.name()) == "xmlns:xacro")
            return true;
    return false;
}

void seed_caller_args(eval_scope &scope, const std::map<std::string, std::string> &args)
{
    for(const std::pair<const std::string, std::string> &arg : args)
        scope.set(arg.first, classify(arg.second));
}

void drive(std::string_view bytes, const std::filesystem::path &path, bool expandable,
           const load_options &opts, parse_context &ctx, world_recorder &recorder,
           capture_window &window)
{
    basic_parser<urdf_reader> parser(ctx);
    if(!expandable)
    {
        parser.parse(bytes, recorder);
        return;
    }
    eval_scope scope;
    seed_caller_args(scope, opts.args);
    scope.install_text_loader(make_yaml_text_loader(ctx.sources, opts.package_roots, ctx.log));
    const expansion expanded = expand(bytes, scope, ctx.sources, path, expansion_limits{},
                                      opts.eval, opts.backend, ctx.log);
    // A failed expansion yields an empty document; parsing it would append a
    // misleading secondary "no document element" error after the real root cause
    // already relayed. Skip the parse only when expand reported that root cause;
    // a quiet failure still parses and fails loudly.
    if(!expanded.ok && window.errors() > 0)
        return;
    parser.parse(expanded.document, recorder);
}

struct sniff_result
{
    std::optional<load_error> error;
    bool expandable;
};

// A record logged through an overload that carries no location names no document, which
// leaves a consumer nothing to resolve; the loaded file is the only file it can be from.
std::vector<captured_diagnostic> anchored(std::vector<captured_diagnostic> records,
                                          const std::filesystem::path &path)
{
    for(captured_diagnostic &record : records)
        if(record.loc.file.empty())
            record.loc.file = path;
    return records;
}

unexpected<load_error> make_error(source_location loc, std::string message, diagnostic_code code,
                                  std::vector<captured_diagnostic> records)
{
    return unexpected<load_error>(
        load_error{ std::move(loc), std::move(message), code, std::move(records) });
}

// Reports an XML-parse failure or a non-<robot> root as a load_error rather than
// logging it; drive_load is the one place that turns such a failure into the load's
// error channel. A clean document reports whether the xacro namespace calls for
// expansion.
sniff_result sniff_robot(std::string_view bytes, const std::filesystem::path &path)
{
    pugi::xml_document probe;
    const unsigned flags = pugi::parse_default | pugi::parse_comments | pugi::parse_ws_pcdata;
    const pugi::xml_parse_result parsed = probe.load_buffer(bytes.data(), bytes.size(), flags);
    if(!parsed)
        return { load_error{ offset_location(bytes, parsed.offset, path),
                             std::string("urdf parse error: ") + parsed.description(),
                             diagnostic_code::xml_parse_error }, false };
    const pugi::xml_node root = first_element(probe);
    if(std::string_view(root.name()) != "robot")
        return { load_error{ node_location(root, bytes, path),
                             "expected a <robot> root element, found <"
                                 + std::string(root.name()) + ">",
                             diagnostic_code::non_robot_root }, false };
    return { std::nullopt, declares_xacro(root) };
}

expected<load_result, load_error> assemble(world_recorder &recorder, capture_window &window,
                                           const std::filesystem::path &path, completeness withheld)
{
    std::vector<captured_diagnostic> records = anchored(window.records(), path);
    if(window.errors() > 0)
    {
        const captured_diagnostic first = *window.first();
        source_location loc = first.loc.file.empty() ? source_location{ path, 0, 0 } : first.loc;
        return make_error(std::move(loc), first.message, first.code, std::move(records));
    }
    const completeness claims = claims_from(records) & ~withheld;
    return load_result{ recorder.take_model(), std::move(records), claims };
}

}

expected<load_result, load_error> drive_load(const std::filesystem::path &path,
                                             const load_options &opts, source_stack &sources,
                                             capture_window &window)
{
    const std::optional<std::string> bytes = read_file(path);
    if(!bytes)
        return make_error(source_location{ path, 0, 0 }, "cannot open input file",
                          diagnostic_code::cannot_open, anchored(window.records(), path));

    const sniff_result sniff = sniff_robot(*bytes, path);
    if(sniff.error)
        return make_error(sniff.error->loc, sniff.error->message, sniff.error->code,
                          anchored(window.records(), path));

    world_recorder recorder(window.log(), opts.topology);
    core_evaluator eval;
    parse_context ctx{ sources, eval, window.log(), opts.on_missing, opts.topology, opts.materials,
                       opts.strict, path, completeness::none, opts.package_roots };
    drive(*bytes, path, sniff.expandable, opts, ctx, recorder, window);
    return assemble(recorder, window, path, ctx.withheld);
}

}
