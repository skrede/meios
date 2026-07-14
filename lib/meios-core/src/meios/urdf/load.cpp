#include "urdf_detail.h"

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

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/source_location.h"

#include <pugixml.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <ostream>
#include <optional>
#include <iostream>
#include <filesystem>
#include <string_view>

namespace meios
{

namespace
{

std::string read_file(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
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

void drive(std::string_view bytes, const std::filesystem::path &path, bool expandable,
           eval_policy policy, evaluator_handle *backend, parse_context &ctx,
           world_recorder &recorder)
{
    basic_parser<urdf_reader> parser(ctx);
    if(!expandable)
    {
        parser.parse(bytes, recorder);
        return;
    }
    eval_scope scope;
    const expansion expanded =
        expand(bytes, scope, ctx.sources, path, expansion_limits{}, policy, backend, ctx.log);
    parser.parse(expanded.document, recorder);
}

// Sniffs the document root: nullopt loud-rejects a non-loadable/non-<robot>
// document, otherwise reports whether the xacro namespace calls for expansion.
std::optional<bool> sniff_robot(std::string_view bytes, const std::filesystem::path &path,
                                log_sink &log)
{
    pugi::xml_document probe;
    const unsigned flags = pugi::parse_default | pugi::parse_comments | pugi::parse_ws_pcdata;
    const pugi::xml_parse_result parsed = probe.load_buffer(bytes.data(), bytes.size(), flags);
    if(!parsed)
    {
        log.log(level::error, std::string("urdf parse error: ") + parsed.description());
        return std::nullopt;
    }
    const pugi::xml_node root = first_element(probe);
    if(std::string_view(root.name()) != "robot")
    {
        log.log(level::error, detail::node_location(root, bytes, path),
                "expected a <robot> root element, found <" + std::string(root.name()) + ">");
        return std::nullopt;
    }
    return declares_xacro(root);
}

model<double> drive_load(const std::filesystem::path &path, const load_options &opts,
                         source_stack &sources, log_sink &log)
{
    const std::string bytes = read_file(path);
    world_recorder recorder(log, opts.topology);

    const std::optional<bool> expandable = sniff_robot(bytes, path, log);
    if(!expandable)
        return recorder.result();

    core_evaluator eval;
    parse_context ctx{ sources, eval, log, opts.on_missing, opts.topology, opts.materials,
                       opts.strict, path };
    drive(bytes, path, *expandable, opts.eval, opts.backend, ctx, recorder);
    return recorder.result();
}

source_stack build_sources(const std::vector<std::filesystem::path> &roots, log_sink &log)
{
    source_stack sources;
    for(const std::filesystem::path &root : roots)
        sources.push_back(source_handle(directory_source(root, log)));
    return sources;
}

}

model<double> load(const std::filesystem::path &path, const load_options &opts, log_sink &log)
{
    source_stack sources = build_sources(opts.package_roots, log);
    return drive_load(path, opts, sources, log);
}

model<double> load(const std::filesystem::path &path, const load_options &opts,
                   source_stack &sources, log_sink &log)
{
    return drive_load(path, opts, sources, log);
}

model<double> load(const std::filesystem::path &path, const load_options &opts)
{
    log_sink_s stderr_sink(std::cerr);
    return load(path, opts, stderr_sink);
}

}
