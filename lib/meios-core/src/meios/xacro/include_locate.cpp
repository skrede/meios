#include "include_locate.h"
#include "include_target.h"

#include "meios/io/source_stack.h"
#include "meios/io/source_handle.h"
#include "meios/io/package_source.h"
#include "meios/io/beside_containment.h"

#include <span>
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

beside_probe probe_status(const std::filesystem::path &candidate, resolved_asset found)
{
    std::error_code error;
    const std::filesystem::file_status status = std::filesystem::status(candidate, error);
    const bool absent = error == std::errc::no_such_file_or_directory || error == std::errc::not_a_directory;
    if(error && !absent)
        return { std::nullopt, operation_failure{ operation_kind::status, error }, false };
    if(absent || !std::filesystem::exists(status))
        return { std::nullopt, std::nullopt, false };
    return { std::move(found), std::nullopt, false };
}

}

// Logs nothing: a miss falls through to the configured sources, and a logged rejection would
// latch as the terminal cause even when one of them then resolves the include.
beside_probe probe_beside(const std::filesystem::path &document, const std::string &spelling, std::span<const std::filesystem::path> roots)
{
    const contained_read_result contained = contain_beside(document, spelling, roots);
    if(!contained)
        return { std::nullopt, contained.error(), false };
    if(!*contained)
        return { std::nullopt, std::nullopt, true };
    const contained_read &found = **contained;
    return probe_status(found.real, resolved_asset{ found.real, found.root, found.real.lexically_relative(found.root) });
}

// A hit carries no source root, so it is read as the operating system opens the path.
beside_probe probe_unbounded(const std::filesystem::path &candidate)
{
    return probe_status(candidate, resolved_asset{ candidate });
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

bool serves_bytes(source_stack &sources, const served_document &served)
{
    return !sources.layers()[served.layer].path_of(served.package, served.relative).has_value();
}

std::optional<served_document> climbed_sibling(const served_document &served, const std::string &written)
{
    bool escaped = false;
    std::string relative = normalize_relative((std::filesystem::path(served.relative).parent_path() / written).generic_string(), escaped);
    if(escaped)
        return std::nullopt;
    return served_document{ served.layer, served.package, std::move(relative) };
}

}
