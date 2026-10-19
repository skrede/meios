#include "include_locate.h"

#include "meios/io/source_stack.h"
#include "meios/io/source_handle.h"
#include "meios/io/package_source.h"
#include "meios/io/directory_source.h"

#include <string>
#include <cstddef>
#include <utility>
#include <optional>
#include <filesystem>
#include <functional>
#include <string_view>
#include <system_error>

namespace meios::detail
{

namespace
{

struct observed_layer
{
    std::reference_wrapper<source_handle> layer;
    std::reference_wrapper<std::optional<std::size_t>> served;
    std::size_t rank;

    capability_descriptor capabilities() const
    {
        return layer.get().capabilities();
    }

    std::optional<resolved_asset> locate(std::string_view package, std::string_view relative)
    {
        std::optional<resolved_asset> found = layer.get().locate(package, relative);
        if(found && !served.get())
            served.get() = rank;
        return found;
    }
};

}

// Logs nothing: a miss falls through to the configured sources, and a logged rejection would
// latch as the terminal cause even when one of them then resolves the include. A hit is read
// under the anchor's real directory, since the confined reader follows no link on its way down.
beside_probe probe_beside(const std::filesystem::path &anchor, const std::string &normalized)
{
    std::error_code error;
    const std::filesystem::path root = std::filesystem::weakly_canonical(anchor, error);
    if(error)
        return { std::nullopt, operation_failure{ operation_kind::canonicalize, error }, false };
    const contained_path_result contained = try_contained_under(root, anchor / normalized);
    if(!contained)
        return { std::nullopt, contained.error(), false };
    if(!*contained)
        return { std::nullopt, std::nullopt, true };
    const std::filesystem::file_status status = std::filesystem::status(**contained, error);
    const bool absent = error == std::errc::no_such_file_or_directory || error == std::errc::not_a_directory;
    if(error && !absent)
        return { std::nullopt, operation_failure{ operation_kind::status, error }, false };
    if(absent || !std::filesystem::exists(status))
        return { std::nullopt, std::nullopt, false };
    return { resolved_asset{ **contained, root, (**contained).lexically_relative(root) }, std::nullopt, false };
}

std::optional<served_hit> locate_served(source_stack &sources, std::string_view package, std::string_view relative, log_sink &log)
{
    std::optional<std::size_t> served;
    source_stack observed;
    for(std::size_t rank = 0; rank < sources.size(); ++rank)
        observed.push_back(source_handle(observed_layer{ sources.layers()[rank], served, rank }));
    std::optional<resolved_asset> hit = observed.locate(package, relative, log);
    if(!hit)
        return std::nullopt;
    return served_hit{ std::move(*hit), *served };
}

served_document sibling_of(const served_document &served, const std::string &normalized)
{
    return served_document{ served.layer, served.package, (std::filesystem::path(served.relative).parent_path() / normalized).generic_string() };
}

std::optional<resolved_asset> probe_served(source_stack &sources, const served_document &sibling)
{
    return sources.layers()[sibling.layer].locate(sibling.package, sibling.relative);
}

}
