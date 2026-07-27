#include "urdf_detail.h"
#include "yaml_resource.h"

#include "meios/urdf/load.h"
#include "meios/urdf/urdf_reader.h"
#include "meios/urdf/parse_context.h"

#include "meios/sink/world_recorder.h"

#include "meios/io/source_stack.h"
#include "meios/io/source_handle.h"
#include "meios/io/directory_source.h"

#include "meios/xacro/budget.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/structural.h"
#include "meios/xacro/core_evaluator.h"
#include "meios/xacro/text_resource_loader.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
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

namespace meios
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
        scope.set(arg.first, detail::classify(arg.second));
}

void drive(std::string_view bytes, const std::filesystem::path &path, bool expandable,
           const load_options &opts, parse_context &ctx, world_recorder &recorder,
           capturing_log_sink &wrapper)
{
    basic_parser<urdf_reader> parser(ctx);
    if(!expandable)
    {
        parser.parse(bytes, recorder);
        return;
    }
    eval_scope scope;
    seed_caller_args(scope, opts.args);
    scope.install_text_loader(detail::make_yaml_text_loader(ctx.sources, opts.package_roots, ctx.log));
    const expansion expanded = expand(bytes, scope, ctx.sources, path, expansion_limits{},
                                      opts.eval, opts.backend, ctx.log);
    // A failed expansion yields an empty document; parsing it would append a
    // misleading secondary "no document element" error after the real root cause
    // already relayed. Skip the parse only when expand reported that root cause;
    // a quiet failure still parses and fails loudly.
    if(!expanded.ok && wrapper.errors() > 0)
        return;
    parser.parse(expanded.document, recorder);
}

struct sniff_result
{
    std::optional<load_error> error;
    bool expandable;
};

unexpected<load_error> make_error(source_location loc, std::string message, diagnostic_code code)
{
    return unexpected<load_error>(load_error{ std::move(loc), std::move(message), code });
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
        return { load_error{ detail::offset_location(bytes, parsed.offset, path),
                             std::string("urdf parse error: ") + parsed.description(),
                             diagnostic_code::xml_parse_error }, false };
    const pugi::xml_node root = first_element(probe);
    if(std::string_view(root.name()) != "robot")
        return { load_error{ detail::node_location(root, bytes, path),
                             "expected a <robot> root element, found <"
                                 + std::string(root.name()) + ">",
                             diagnostic_code::non_robot_root }, false };
    return { std::nullopt, declares_xacro(root) };
}

expected<model<double>, load_error> drive_load(const std::filesystem::path &path,
                                               const load_options &opts, source_stack &sources,
                                               capturing_log_sink &wrapper)
{
    const std::optional<std::string> bytes = read_file(path);
    if(!bytes)
        return make_error(source_location{ path, 0, 0 }, "cannot open input file",
                          diagnostic_code::cannot_open);

    const sniff_result sniff = sniff_robot(*bytes, path);
    if(sniff.error)
        return unexpected<load_error>(*sniff.error);

    world_recorder recorder(wrapper, opts.topology);
    core_evaluator eval;
    parse_context ctx{ sources, eval, wrapper, opts.on_missing, opts.topology, opts.materials,
                       opts.strict, path };
    drive(*bytes, path, sniff.expandable, opts, ctx, recorder, wrapper);
    if(wrapper.errors() > 0)
    {
        const captured_diagnostic &first = *wrapper.first();
        source_location loc = first.loc.file.empty() ? source_location{ path, 0, 0 } : first.loc;
        return make_error(std::move(loc), first.message, first.code);
    }
    return recorder.take_model();
}

source_stack build_sources(const std::vector<std::filesystem::path> &roots, log_sink &log)
{
    source_stack sources;
    for(const std::filesystem::path &root : roots)
        sources.push_back(source_handle(directory_source(root, log)));
    return sources;
}

}

expected<model<double>, load_error> load(const std::filesystem::path &path,
                                         const load_options &opts, log_sink &log)
{
    // Wrap before building sources so a directory_source containment rejection, which
    // it emits to the sink it was constructed with, is counted at the load boundary
    // rather than escaping to the raw sink as a silent error.
    capturing_log_sink wrapper(log);
    source_stack sources = build_sources(opts.package_roots, wrapper);
    return drive_load(path, opts, sources, wrapper);
}

expected<model<double>, load_error> load(const std::filesystem::path &path,
                                         const load_options &opts, source_stack &sources,
                                         log_sink &log)
{
    capturing_log_sink wrapper(log);
    return drive_load(path, opts, sources, wrapper);
}

expected<model<double>, load_error> load(const std::filesystem::path &path, const load_options &opts)
{
    log_sink silent;
    return load(path, opts, silent);
}

}
