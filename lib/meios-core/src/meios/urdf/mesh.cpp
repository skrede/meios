#include "lfs_pointer.h"
#include "urdf_detail.h"

#include "meios/records/geometry.h"

#include "meios/io/source_stack.h"
#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <string>
#include <utility>
#include <optional>
#include <string_view>

namespace meios::detail
{

namespace
{

void report_missing(parse_context &ctx, const source_location &loc, const std::string &uri)
{
    if(ctx.on_missing == missing_asset::skip)
        return;
    const level lvl = ctx.on_missing == missing_asset::fail ? level::error : level::warn;
    ctx.log.log(lvl, diagnostic_code::unresolved_mesh, loc, "could not resolve mesh '" + uri + "'");
}

// A materialized temp file is unlinked when the resolved_asset that owns it dies, and no part of
// the returned model can hold that ownership, so recording its path would name a file already
// gone. Failing loudly keeps a byte-backed source from yielding a model that looks resolved.
void record_resolved(mesh<double> &shape, resolved_asset &&asset, parse_context &ctx,
                     const source_location &loc)
{
    if(asset.holds_path())
    {
        shape.resolved_path = asset.path().string();
        return;
    }
    ctx.log.log(level::error, diagnostic_code::unresolved_mesh, loc,
                "mesh '" + shape.filename
                    + "' resolves to a byte-backed source; meios cannot yet hand out a path that "
                      "outlives the load");
}

}

void resolve_mesh(mesh<double> &shape, parse_context &ctx, const source_location &loc)
{
    constexpr std::string_view prefix = "package://";
    std::string_view uri = shape.filename;
    if(!uri.starts_with(prefix))
        return;
    uri.remove_prefix(prefix.size());
    const std::size_t slash = uri.find('/');
    if(slash == std::string_view::npos)
    {
        ctx.log.log(level::error, diagnostic_code::malformed_mesh_uri, loc,
                    "malformed package:// mesh URI '" + shape.filename + "'");
        return;
    }
    const std::string pkg(uri.substr(0, slash));
    const std::string rel(uri.substr(slash + 1));
    std::optional<resolved_asset> hit = ctx.sources.locate(pkg, rel, ctx.log);
    if(!hit)
    {
        report_missing(ctx, loc, shape.filename);
        return;
    }
    record_resolved(shape, std::move(*hit), ctx, loc);
    if(shape.resolved_path && is_lfs_pointer(*shape.resolved_path))
    {
        ctx.log.log(level::error, diagnostic_code::lfs_pointer_mesh, loc,
                    "resolved mesh '" + shape.filename + "' is an unsmudged Git-LFS pointer, not geometry");
        shape.resolved_path.reset();
    }
}

}
