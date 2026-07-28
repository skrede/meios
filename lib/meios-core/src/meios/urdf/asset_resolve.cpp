#include "asset_uri.h"
#include "lfs_pointer.h"

#include "meios/io/source_stack.h"
#include "meios/io/resolved_asset.h"
#include "meios/io/directory_source.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/claims.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <string>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace meios::detail
{

namespace
{

void report_missing(parse_context &ctx, const source_location &loc, const std::string &uri)
{
    if(ctx.on_missing == missing_asset::skip)
    {
        ctx.withheld |= cleared_by(diagnostic_code::unresolved_asset);
        return;
    }
    const level lvl = ctx.on_missing == missing_asset::fail ? level::error : level::warn;
    ctx.log.log(lvl, diagnostic_code::unresolved_asset, loc,
                "could not resolve asset '" + uri + "'");
}

// A document named by a bare relative path has an empty parent, and an empty root contains
// nothing, so the base is made absolute before anything is joined against or checked against
// it; otherwise a relative asset false-refuses against the very directory it was written in.
std::filesystem::path document_base(const parse_context &ctx)
{
    std::error_code ec;
    const std::filesystem::path full = std::filesystem::absolute(ctx.document, ec);
    return ec ? ctx.document.parent_path() : full.parent_path();
}

std::optional<std::filesystem::path> first_container(const std::filesystem::path &candidate,
                                                     const parse_context &ctx)
{
    for(const std::filesystem::path &root : ctx.package_roots)
    {
        if(const std::optional<std::filesystem::path> real = contained_under(root, candidate))
            return real;
    }
    return contained_under(document_base(ctx), candidate);
}

// A path no root contains is unreachable and a path a root contains but the filesystem does
// not hold is absent; only the second is the missing-asset policy's business.
std::optional<std::string> resolve_contained(const std::filesystem::path &candidate,
                                             parse_context &ctx, const source_location &loc,
                                             const std::string &uri)
{
    const std::optional<std::filesystem::path> real = first_container(candidate, ctx);
    if(!real)
    {
        report_unreachable(ctx, loc, uri);
        return std::nullopt;
    }
    std::error_code ec;
    if(!std::filesystem::exists(*real, ec))
    {
        report_missing(ctx, loc, uri);
        return std::nullopt;
    }
    return real->string();
}

std::optional<std::string> resolve_package(const std::string &uri, parse_context &ctx,
                                           const source_location &loc)
{
    const std::string_view rest =
        std::string_view(uri).substr(std::string_view("package://").size());
    const std::size_t slash = rest.find('/');
    if(slash == std::string_view::npos)
    {
        ctx.log.log(level::error, diagnostic_code::malformed_asset_uri, loc,
                    "malformed package:// asset URI '" + uri + "'");
        return std::nullopt;
    }
    const std::optional<resolved_asset> hit =
        ctx.sources.locate(rest.substr(0, slash), rest.substr(slash + 1), ctx.log);
    if(!hit)
    {
        report_missing(ctx, loc, uri);
        return std::nullopt;
    }
    return hit->path().string();
}

std::optional<std::string> resolve_absolute(const std::string &uri, parse_context &ctx,
                                            const source_location &loc)
{
    const std::filesystem::path candidate =
        scheme_of(uri) ? std::filesystem::path(file_uri_to_path(uri)) : std::filesystem::path(uri);
    return resolve_contained(candidate, ctx, loc, uri);
}

// The form a path was written in must not change what it is allowed to reach, so a relative
// path is joined against the top-level input document's directory and then held to the same
// containment rule an absolute path is.
std::optional<std::string> resolve_relative(const std::string &uri, parse_context &ctx,
                                            const source_location &loc)
{
    return resolve_contained(document_base(ctx) / uri, ctx, loc, uri);
}

std::optional<std::string> resolve_by_form(const std::string &uri, parse_context &ctx,
                                           const source_location &loc)
{
    switch(classify_asset_uri(uri))
    {
        case asset_uri_form::package:  return resolve_package(uri, ctx, loc);
        case asset_uri_form::relative: return resolve_relative(uri, ctx, loc);
        case asset_uri_form::absolute: return resolve_absolute(uri, ctx, loc);
        case asset_uri_form::foreign_scheme: break;
    }
    report_foreign_scheme(ctx, loc, uri, *scheme_of(uri));
    return std::nullopt;
}

}

void report_unreachable(parse_context &ctx, const source_location &loc, const std::string &uri)
{
    ctx.log.log(level::error, diagnostic_code::uncontained_asset, loc,
                "asset '" + uri + "' resolves outside every configured root");
}

void report_foreign_scheme(parse_context &ctx, const source_location &loc, const std::string &uri,
                           std::string_view scheme)
{
    ctx.log.log(level::error, diagnostic_code::unsupported_uri_scheme, loc,
                "asset '" + uri + "' names the unsupported URI scheme '" + std::string(scheme)
                    + "'");
}

std::optional<std::string> resolve_asset_uri(const std::string &uri, parse_context &ctx,
                                             const source_location &loc)
{
    const std::optional<std::string> path = resolve_by_form(uri, ctx, loc);
    if(!path || !is_lfs_pointer(*path))
        return path;
    ctx.log.log(level::error, diagnostic_code::lfs_pointer_asset, loc,
                "resolved asset '" + uri + "' is an unsmudged Git-LFS pointer, not geometry");
    return std::nullopt;
}

}
