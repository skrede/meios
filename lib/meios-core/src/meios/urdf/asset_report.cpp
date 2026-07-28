#include "asset_report.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/claims.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <string>
#include <string_view>

namespace meios::detail
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

void report_foreign_authority(parse_context &ctx, const source_location &loc,
                              const std::string &uri, std::string_view authority)
{
    ctx.log.log(level::error, diagnostic_code::unsupported_uri_authority, loc,
                "asset '" + uri + "' names the host '" + std::string(authority)
                    + "', which meios does not reach");
}

}
