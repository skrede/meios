#include "asset_uri.h"
#include "urdf_detail.h"

#include "meios/records/geometry.h"

namespace meios::detail
{

void resolve_mesh(mesh<double> &shape, parse_context &ctx, const source_location &loc)
{
    shape.resolved_path = resolve_asset_uri(shape.filename, ctx, loc);
}

}
