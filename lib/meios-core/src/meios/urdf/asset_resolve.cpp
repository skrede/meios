#include "asset_uri.h"
#include "lfs_pointer.h"
#include "asset_report.h"

#include "meios/io/source_stack.h"
#include "meios/io/source_lookup.h"
#include "meios/io/resolved_asset.h"
#include "meios/io/directory_source.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <string>
#include <cstddef>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

namespace
{

// The base is made absolute so a document named by a bare relative path does not false-refuse
// the siblings it was written beside. A document with no path contributes an empty base, so a
// byte-backed document's relative references probe only the configured roots.
std::filesystem::path document_base(const parse_context &ctx)
{
    return absolute_base(ctx.document).parent_path();
}

std::optional<std::filesystem::path> first_container(const std::filesystem::path &candidate, const parse_context &ctx)
{
    for(const std::filesystem::path &root : ctx.package_roots)
    {
        if(const std::optional<std::filesystem::path> real = contained_under(absolute_base(root), candidate))
            return real;
    }
    return contained_under(document_base(ctx), candidate);
}

// A path no root contains is unreachable and a path a root contains but the filesystem does
// not hold as a regular file is absent; only the second is the missing-asset policy's business.
std::optional<std::string> resolve_contained(const std::filesystem::path &candidate, parse_context &ctx, const source_location &loc, const std::string &uri)
{
    const std::optional<std::filesystem::path> real = first_container(candidate, ctx);
    if(!real)
    {
        report_unreachable(ctx, loc, uri);
        return std::nullopt;
    }
    return asset_or_missing(*real, ctx, loc, uri);
}

std::optional<std::string> package_result(const std::string &uri, source_lookup_result hit, parse_context &ctx, const source_location &loc)
{
    if(!hit)
    {
        const operation_failure &cause = hit.error();
        ctx.log.log(level::error, diagnostic_code::unresolved_asset, loc, cause, "cannot resolve asset '" + uri + "': " + cause.native.message());
        return std::nullopt;
    }
    if(!*hit)
    {
        report_missing(ctx, loc, uri);
        return std::nullopt;
    }
    return asset_or_missing((*hit)->path(), ctx, loc, uri);
}

std::optional<std::string> resolve_package(const std::string &uri, parse_context &ctx, const source_location &loc)
{
    const std::string_view rest = std::string_view(uri).substr(std::string_view("package://").size());
    const std::size_t slash     = rest.find('/');
    if(slash == std::string_view::npos || slash == 0 || slash + 1 == rest.size())
    {
        ctx.log.log(level::error, diagnostic_code::malformed_asset_uri, loc, "malformed package:// asset URI '" + uri + "'");
        return std::nullopt;
    }
    return package_result(uri, ctx.sources.try_locate(rest.substr(0, slash), rest.substr(slash + 1), ctx.log), ctx, loc);
}

char ascii_lower(char c)
{
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

// RFC 8089 section 2 names "localhost" as equivalent to an empty authority: both are the local
// filesystem. A URI host is case-insensitive, and the comparison stays here rather than in the
// parser so the helper reports what a URI carries and this decides what it means.
bool local_host(std::string_view authority)
{
    constexpr std::string_view name = "localhost";
    return std::equal(authority.begin(), authority.end(), name.begin(), name.end(), [](char a, char b) { return ascii_lower(a) == b; });
}

// The authority is decided before the path reaches containment. Left in place it survives as
// the first component of a relative path, which is then resolved against the process working
// directory, so the same document loads differently depending on where it was loaded from.
std::optional<std::filesystem::path> file_uri_candidate(const std::string &uri, parse_context &ctx, const source_location &loc)
{
    const std::string_view authority = file_authority(uri);
    if(!authority.empty() && !local_host(authority))
    {
        report_foreign_authority(ctx, loc, uri, authority);
        return std::nullopt;
    }
    const std::size_t after = std::string_view("file://").size() + authority.size();
    return std::filesystem::path(file_uri_to_path("file://" + uri.substr(after)));
}

std::optional<std::string> resolve_absolute(const std::string &uri, bool file_scheme, parse_context &ctx, const source_location &loc)
{
    std::filesystem::path candidate(uri);
    if(file_scheme)
    {
        const std::optional<std::filesystem::path> named = file_uri_candidate(uri, ctx, loc);
        if(!named)
            return std::nullopt;
        candidate = *named;
    }
    if(!candidate.is_absolute())
    {
        ctx.log.log(level::error, diagnostic_code::malformed_asset_uri, loc, "asset '" + uri + "' does not normalize to an absolute path");
        return std::nullopt;
    }
    return resolve_contained(candidate, ctx, loc, uri);
}

// The form a path was written in must not change what it is allowed to reach, so a relative
// path is joined against the top-level input document's directory and then held to the same
// containment rule an absolute path is.
std::optional<std::string> resolve_relative(const std::string &uri, parse_context &ctx, const source_location &loc)
{
    return resolve_contained(document_base(ctx) / uri, ctx, loc, uri);
}

// The scheme is resolved once and carried into the arms that need it. Each arm recomputing it
// was correct only while the classifier established the invariant one frame up, which put an
// unchecked dereference one classifier edit away from being reachable.
std::optional<std::string> resolve_by_form(const std::string &uri, parse_context &ctx, const source_location &loc)
{
    const std::optional<std::string_view> scheme = scheme_of(uri);
    switch(classify_asset_uri(uri))
    {
        case asset_uri_form::package:
            return resolve_package(uri, ctx, loc);
        case asset_uri_form::relative:
            return resolve_relative(uri, ctx, loc);
        case asset_uri_form::absolute:
            return resolve_absolute(uri, scheme.has_value(), ctx, loc);
        case asset_uri_form::foreign_scheme:
            break;
    }
    report_foreign_scheme(ctx, loc, uri, scheme.value_or(std::string_view{}));
    return std::nullopt;
}

}

// The empty guard sits here rather than in each reader because this is the one function every
// element reaches: an empty reference joined onto the document base normalizes back to that
// directory, which a root contains and which exists, so every later check answers yes to a
// question that should never have been asked.
std::optional<std::string> resolve_asset_uri(const std::string &uri, parse_context &ctx, const source_location &loc)
{
    if(uri.empty())
    {
        ctx.log.log(level::error, diagnostic_code::malformed_asset_uri, loc, "asset reference is empty");
        return std::nullopt;
    }
    std::optional<std::string> path = resolve_by_form(uri, ctx, loc);
    if(!path || !is_lfs_pointer(*path))
        return path;
    ctx.log.log(level::error, diagnostic_code::lfs_pointer_asset, loc, "resolved asset '" + uri + "' is an unsmudged Git-LFS pointer, not geometry");
    return std::nullopt;
}

}
