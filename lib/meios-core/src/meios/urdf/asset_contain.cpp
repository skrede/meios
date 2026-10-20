#include "asset_uri.h"
#include "asset_report.h"

#include "meios/io/directory_source.h"
#include "meios/io/beside_containment.h"

#include <string>
#include <optional>
#include <filesystem>
#include <system_error>

namespace meios::detail
{

namespace
{

contained_path_result first_container(const std::filesystem::path &candidate, const parse_context &ctx)
{
    const contained_read_result contained = contain_beside(absolute_base(ctx.document), candidate, ctx.package_roots);
    if(!contained)
        return unexpected<operation_failure>(contained.error());
    if(!*contained)
        return std::optional<std::filesystem::path>{};
    return std::optional{(*contained)->real};
}

std::optional<std::filesystem::path> followed_link(const std::filesystem::path &spelling, const parse_context &ctx)
{
    if(!ctx.includes.follow_outside_links || ctx.document.empty())
        return std::nullopt;
    return beside_link(absolute_base(ctx.document), spelling);
}

std::optional<std::string> resolve_followed(const std::filesystem::path &link, parse_context &ctx, const source_location &loc, const std::string &uri)
{
    std::error_code error;
    const std::filesystem::path target = std::filesystem::weakly_canonical(link, error);
    if(error)
        return report_cause(ctx, loc, uri, operation_failure{operation_kind::canonicalize, error});
    return asset_or_missing(target, ctx, loc, uri);
}

}

std::optional<std::string> resolve_contained(const std::filesystem::path &candidate, const std::filesystem::path &spelling, parse_context &ctx, const source_location &loc,
                                             const std::string &uri)
{
    const contained_path_result real = first_container(candidate, ctx);
    if(!real)
        return report_cause(ctx, loc, uri, real.error());
    if(*real)
        return asset_or_missing(**real, ctx, loc, uri);
    if(const std::optional<std::filesystem::path> link = followed_link(spelling, ctx))
        return resolve_followed(*link, ctx, loc, uri);
    report_unreachable(ctx, loc, uri);
    return std::nullopt;
}

}
