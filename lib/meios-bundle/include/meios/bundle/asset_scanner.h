#ifndef HPP_GUARD_MEIOS_BUNDLE_ASSET_SCANNER_H
#define HPP_GUARD_MEIOS_BUNDLE_ASSET_SCANNER_H

#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/log_sink.h"

#include <string>
#include <vector>
#include <concepts>

namespace meios
{

template <typename S>
concept asset_scanner = requires(S s, const resolved_asset &a, log_sink &log)
{
    { s.scan(a, log) } -> std::convertible_to<std::vector<std::string>>;
};

}

#endif
