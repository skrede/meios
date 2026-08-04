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

inline text_read_result read_asset_bytes(const resolved_asset &asset)
{
    if(asset.source_root() && asset.source_relative())
        return read_text_file_under(*asset.source_root(), *asset.source_relative());
    return read_text_file(asset.path());
}

// A refusal by kind carries no native code, so naming the classification is what keeps the
// message from reporting a failure whose stated reason is that nothing went wrong.
inline std::string read_failure_reason(const text_read_failure &failure)
{
    if(failure.kind == text_read_failure_kind::non_regular)
        return "not a regular file";
    return std::string(to_string(failure.cause.operation)) + " failed: " + failure.cause.native.message();
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
