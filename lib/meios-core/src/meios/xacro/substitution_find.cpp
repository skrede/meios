#include "substitution_lex.h"
#include "substitution_find.h"
#include "substitution_detail.h"

#include "meios/io/source_stack.h"
#include "meios/io/source_handle.h"
#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/diagnostic_code.h"

#include <string>
#include <optional>
#include <algorithm>
#include <string_view>

namespace meios::detail
{

namespace
{

std::optional<std::string> refuse(subst_ctx &ctx, diagnostic_code code, const std::string &message)
{
    refuse_terminal(ctx, code, message);
    return std::nullopt;
}

std::optional<std::string> unresolved(subst_ctx &ctx, std::string_view package)
{
    return refuse(ctx, diagnostic_code::unresolved_find, "$(find " + std::string(package) + ") did not resolve");
}

std::string named_text(const resolved_asset &hit)
{
    return hit.named().value_or(hit.path()).generic_string();
}

// Each layer is asked quietly and only the one that answers locates, so the find adds no record
// beyond those the include's own lookup makes.
std::optional<std::string> path_named(source_stack &sources, std::string_view package)
{
    for(source_handle &layer : sources.layers())
    {
        if(!layer.names_paths() || !layer.answers(package, ""))
            continue;
        if(const std::optional<resolved_asset> hit = layer.locate(package, ""))
            return named_text(*hit);
    }
    return std::nullopt;
}

bool configures_byte_layer(const source_stack &sources)
{
    return std::ranges::any_of(sources.layers(), [](const source_handle &layer) { return !layer.names_paths(); });
}

std::optional<std::string> find_for_include(subst_ctx &ctx, include_record &record, std::string_view package)
{
    if(std::optional<std::string> directory = path_named(ctx.sources, package))
    {
        record.found.push_back(found_package{ std::string(package), *directory, false });
        return directory;
    }
    if(!configures_byte_layer(ctx.sources))
        return unresolved(ctx, package);
    if(record.span_offset != 0)
        return refuse(ctx, diagnostic_code::unresolved_include, names_no_directory(package));
    record.found.push_back(found_package{ std::string(package), {}, true });
    return std::string{};
}

}

std::string names_no_directory(std::string_view package)
{
    const std::string find = "$(find " + std::string(package) + ')';
    return find + " names no directory: no layer that names paths holds package \"" + std::string(package)
           + "\", and a layer that serves only bytes is asked for a file only through a leading " + find + " followed by a separator";
}

// Upstream splits a command on spaces and refuses a find unless exactly one argument remains.
std::optional<std::string> cmd_find(subst_ctx &ctx, std::string_view rest)
{
    const std::string_view package = rest.substr(0, rest.find_first_of(" \t"));
    if(package.empty())
        return refuse(ctx, diagnostic_code::unresolved_find, "$(find) requires a package name");
    if(package.size() != rest.size())
        return refuse(ctx, diagnostic_code::unresolved_find,
                      "$(find " + std::string(package) + ") takes exactly one package name, but was also given \""
                          + std::string(trim(rest.substr(package.size()))) + '"');
    if(ctx.include)
        return find_for_include(ctx, *ctx.include, package);
    const std::optional<resolved_asset> hit = ctx.sources.locate(package, "", ctx.log);
    if(!hit)
        return unresolved(ctx, package);
    return named_text(*hit);
}

}
