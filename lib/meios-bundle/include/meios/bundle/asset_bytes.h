#ifndef HPP_GUARD_MEIOS_BUNDLE_ASSET_BYTES_H
#define HPP_GUARD_MEIOS_BUNDLE_ASSET_BYTES_H

#include "meios/io/text_reader.h"
#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"

#include <string>

namespace meios
{

namespace detail
{

// meios::detail also declares two-argument readers of both these names, so an unqualified call
// here would resolve to whichever declaration a translation unit happened to see first.
inline text_read_result read_asset_bytes(const resolved_asset &asset)
{
    if(asset.source_root() && asset.source_relative())
        return meios::read_text_file_under(*asset.source_root(), *asset.source_relative());
    return meios::read_text_file(asset.path());
}

inline void report_asset_read_failure(const resolved_asset &asset, const text_read_failure &failure, log_sink &log)
{
    log.log(level::error, diagnostic_code::cannot_open, source_location{}, failure.cause,
            "cannot read asset \"" + asset.path().string() + "\": " + read_failure_reason(failure));
}

}

// The one reporting site for a failed asset read: a scanner branches on the error arm and
// reports nothing, so a terminal failure yields exactly one record.
inline text_read_result read_asset_text(const resolved_asset &asset, log_sink &log)
{
    text_read_result text = detail::read_asset_bytes(asset);
    if(!text)
        detail::report_asset_read_failure(asset, text.error(), log);
    return text;
}

}

#endif
