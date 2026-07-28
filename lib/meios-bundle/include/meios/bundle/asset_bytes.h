#ifndef HPP_GUARD_MEIOS_BUNDLE_ASSET_BYTES_H
#define HPP_GUARD_MEIOS_BUNDLE_ASSET_BYTES_H

#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <string>
#include <fstream>
#include <iterator>

namespace meios
{

inline std::string read_asset_text(const resolved_asset &asset, log_sink &log)
{
    std::ifstream in(asset.path(), std::ios::binary);
    if(!in)
    {
        log.log(level::warn, "could not open asset '" + asset.path().string() + "'");
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

}

#endif
