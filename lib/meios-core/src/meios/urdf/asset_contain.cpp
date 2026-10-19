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

// A root the filesystem could not traverse neither contains nor excludes the candidate: it
// retains its failure for an exhausted search and yields to the next root, which discards it.
bool contains(const std::filesystem::path &root, const std::filesystem::path &candidate, std::optional<std::filesystem::path> &real, std::optional<operation_failure> &retained)
{
    contained_path_result under = try_contained_under(root, candidate);
    if(!under)
    {
        if(!retained)
            retained = under.error();
        return false;
    }
    real = std::move(*under);
    return real.has_value();
}

// The document's directory is judged as it was named and as where the document really lives,
// the rule an include and an auxiliary document beside it are held to.
bool beside_document(const std::filesystem::path &candidate, const parse_context &ctx, std::optional<std::filesystem::path> &real, std::optional<operation_failure> &retained)
{
    if(ctx.document.empty())
        return false;
    const contained_read_result beside = contain_beside(absolute_base(ctx.document), candidate);
    if(!beside)
    {
        if(!retained)
            retained = beside.error();
        return false;
    }
    if(*beside)
        real = (*beside)->real;
    return real.has_value();
}

contained_path_result first_container(const std::filesystem::path &candidate, const parse_context &ctx)
{
    std::optional<std::filesystem::path> real;
    std::optional<operation_failure> retained;
    for(const std::filesystem::path &root : ctx.package_roots)
        if(contains(absolute_base(root), candidate, real, retained))
            return real;
    if(beside_document(candidate, ctx, real, retained))
        return real;
    if(retained)
        return unexpected<operation_failure>(*retained);
    return std::optional<std::filesystem::path>{};
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
