#include "expand_context.h"
#include "include_target.h"
#include "include_spelling.h"
#include "substitution_find.h"
#include "substitution_detail.h"

#include "meios/expected.h"

#include "meios/diagnostic/expansion_error.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <pugixml.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

namespace
{

struct spelled
{
    std::string text;
    std::vector<found_package> found;
};

struct package_hit
{
    std::string package;
    std::size_t end;
};

expected<spelled, expansion_error> substitute_whole(expand_ctx &ctx, pugi::xml_node in, std::string_view raw, const std::filesystem::path &document)
{
    subst_ctx whole(ctx.log, ctx.sources, ctx.scope, document, ctx.mode, ctx.backend, ctx.session, locate(ctx, in));
    whole.include = include_record{ {}, 0 };
    std::string text;
    if(!scan(whole, raw, text, 0))
        return unexpected<expansion_error>(whole.terminal.value_or(expansion_error{}));
    return spelled{ std::move(text), std::move(whole.include->found) };
}

std::string after_separator(std::string_view text)
{
    if(!text.empty() && is_separator(text.front()))
        text.remove_prefix(1);
    return std::string(text);
}

std::optional<package_hit> under_found(const std::vector<found_package> &found, std::string_view text)
{
    std::optional<package_hit> longest;
    for(const found_package &each : found)
    {
        const std::optional<std::size_t> end = directory_prefix(text, each.directory);
        if(end && (!longest || *end > longest->end))
            longest = package_hit{ each.package, *end };
    }
    return longest;
}

// A layer that serves only bytes names no directory, so only a leading find followed by a separator
// can name its file.
std::optional<include_target> byte_served_route(expand_ctx &ctx, pugi::xml_node in, std::string_view raw, const spelled &whole, const found_package &served)
{
    if(whole.found.size() == 1 && (whole.text.empty() || is_separator(whole.text.front())))
        return include_target{ served.package, after_separator(whole.text) };
    fail(ctx, in, diagnostic_code::unresolved_include, "xacro:include could not resolve \"" + std::string(raw) + "\": " + names_no_directory(served.package));
    return std::nullopt;
}

// A filename is resolved as a path, and a path under a directory a find rendered is asked of the
// sources as that package's file, as the package route asked it.
std::optional<include_target> route(expand_ctx &ctx, pugi::xml_node in, std::string_view raw, spelled whole)
{
    const std::vector<found_package>::const_iterator served = std::ranges::find_if(whole.found, &found_package::byte_served);
    if(served != whole.found.end())
        return byte_served_route(ctx, in, raw, whole, *served);
    if(const std::optional<package_hit> hit = under_found(whole.found, whole.text))
        return include_target{ hit->package, after_separator(std::string_view(whole.text).substr(hit->end)) };
    return include_target{ {}, std::move(whole.text) };
}

}

std::optional<include_target> spell_include(expand_ctx &ctx, pugi::xml_node in, std::string_view raw, const std::filesystem::path &document)
{
    expected<spelled, expansion_error> whole = substitute_whole(ctx, in, raw, document);
    if(!whole)
    {
        record_terminal(ctx, whole.error());
        ctx.ok = false;
        return std::nullopt;
    }
    return route(ctx, in, raw, std::move(*whole));
}

}
