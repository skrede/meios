#include "load_stage.h"
#include "urdf_detail.h"
#include "load_acquire.h"
#include "yaml_resource.h"

#include "meios/io/text_reader_operations.h"

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
#include "meios/diagnostic/expansion_error.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"
#include "meios/diagnostic/capturing_log_sink.h"

#include <pugixml.hpp>

#include <map>
#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

namespace
{

pugi::xml_node first_element(const pugi::xml_node &document)
{
    for(pugi::xml_node child : document.children())
        if(child.type() == pugi::node_element)
            return child;
    return {};
}

bool declares_xacro(pugi::xml_node root)
{
    return std::ranges::any_of(root.attributes(), [](pugi::xml_attribute attr) { return std::string_view(attr.name()) == "xmlns:xacro"; });
}

void seed_caller_args(eval_scope &scope, const std::map<std::string, std::string> &args)
{
    for(const std::pair<const std::string, std::string> &arg : args)
        scope.set(arg.first, classify(strip_authored_markers(arg.second)));
}

std::optional<expansion_error> drive(std::string_view bytes, const std::filesystem::path &path, bool expandable, const load_options &opts, parse_context &ctx, world_recorder &recorder)
{
    basic_parser<urdf_reader> parser(ctx);
    if(!expandable)
    {
        parser.parse(bytes, recorder);
        return std::nullopt;
    }
    eval_scope scope;
    seed_caller_args(scope, opts.args);
    scope.install_text_loader(make_yaml_text_loader(ctx.sources, opts.package_roots, ctx.log));
    const expected<expansion, expansion_error> expanded = expand(bytes, scope, ctx.sources, path, expansion_limits{}, opts.eval, opts.backend, ctx.log);
    if(!expanded)
        return expanded.error();
    parser.parse(expanded->document, recorder);
    return std::nullopt;
}

struct sniff_result
{
    sniff_result(std::optional<load_error> error_value, bool expandable_value)
            : error(std::move(error_value))
            , expandable(expandable_value)
    {
    }

    std::optional<load_error> error;
    bool expandable;
};

// A record logged through an overload that carries no location names no document, which
// leaves a consumer nothing to resolve; the loaded file is the only file it can be from.
std::vector<captured_diagnostic> anchored(std::vector<captured_diagnostic> records, const std::filesystem::path &path)
{
    for(captured_diagnostic &record : records)
        if(record.loc.file.empty())
            record.loc.file = path;
    return records;
}

unexpected<load_error> make_error(source_location loc, std::string message, diagnostic_code code, std::vector<captured_diagnostic> records,
                                  std::optional<operation_failure> cause = std::nullopt)
{
    return unexpected<load_error>(load_error{std::move(loc), std::move(message), code, std::move(records), cause});
}

// Reports an XML-parse failure or a non-<robot> root as a load_error rather than
// logging it; drive_load is the one place that turns such a failure into the load's
// error channel. A clean document reports whether the xacro namespace calls for
// expansion.
sniff_result sniff_robot(std::string_view bytes, const std::filesystem::path &path)
{
    pugi::xml_document probe;
    const unsigned flags                = pugi::parse_default | pugi::parse_comments | pugi::parse_ws_pcdata;
    const pugi::xml_parse_result parsed = probe.load_buffer(bytes.data(), bytes.size(), flags);
    if(!parsed)
        return {load_error{offset_location(bytes, parsed.offset, path), std::string("urdf parse error: ") + parsed.description(), diagnostic_code::xml_parse_error}, false};
    const pugi::xml_node root = first_element(probe);
    if(std::string_view(root.name()) != "robot")
        return {load_error{node_location(root, bytes, path), "expected a <robot> root element, found <" + std::string(root.name()) + ">", diagnostic_code::non_robot_root}, false};
    return {std::nullopt, declares_xacro(root)};
}

expected<load_result, load_error> assemble(world_recorder &recorder, capture_window &window, const std::filesystem::path &path, completeness withheld)
{
    std::vector<captured_diagnostic> records = anchored(window.records(), path);
    if(window.errors() > 0)
    {
        const captured_diagnostic first = *window.first();
        source_location loc             = first.loc.file.empty() ? source_location{path, 0, 0} : first.loc;
        return make_error(std::move(loc), first.message, first.code, std::move(records), first.cause);
    }
    const completeness claims = claims_from(records) & ~withheld;
    return load_result{recorder.take_model(), std::move(records), claims};
}

class null_stage_probe final : public load_stage_probe
{
public:
    void entered(load_stage_point) override
    {
    }
};

}

expected<load_result, load_error> drive_load(const std::filesystem::path &path, const load_options &opts, source_stack &sources, capture_window &window,
                                             const text_reader_operations &operations, load_stage_probe &probe)
{
    expected<std::string, load_error> bytes = acquire_load_text(path, window, operations);
    if(!bytes)
        return unexpected<load_error>(std::move(bytes.error()));

    probe.entered(load_stage_point::sniff);
    const sniff_result sniff = sniff_robot(*bytes, path);
    if(sniff.error)
        return make_error(sniff.error->loc, sniff.error->message, sniff.error->code, anchored(window.records(), path));

    world_recorder recorder(window.log(), opts.topology);
    core_evaluator eval;
    parse_context ctx{sources, eval, window.log(), opts.on_missing, opts.topology, opts.materials, opts.strict, path, completeness::none, opts.package_roots};
    probe.entered(load_stage_point::drive);
    const std::optional<expansion_error> refused = drive(*bytes, path, sniff.expandable, opts, ctx, recorder);
    probe.entered(load_stage_point::assemble);
    if(refused)
        return make_error(refused->loc, refused->message, refused->code, anchored(window.records(), path), refused->cause);
    return assemble(recorder, window, path, ctx.withheld);
}

expected<load_result, load_error> drive_load(const std::filesystem::path &path, const load_options &opts, source_stack &sources, capture_window &window)
{
    null_stage_probe probe;
    return drive_load(path, opts, sources, window, default_text_reader_operations(), probe);
}

}
