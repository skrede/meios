#ifndef HPP_GUARD_MEIOS_BUNDLE_ASSET_BYTES_H
#define HPP_GUARD_MEIOS_BUNDLE_ASSET_BYTES_H

#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <span>
#include <array>
#include <string>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <filesystem>

namespace meios
{

namespace detail
{

inline std::string read_path_text(const std::filesystem::path &path, log_sink &log)
{
    std::ifstream in(path, std::ios::binary);
    if(!in)
    {
        log.log(level::warn, "could not open asset '" + path.string() + "'");
        return {};
    }
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

inline std::string drain_bytes(byte_reader &reader)
{
    std::string text;
    std::array<std::byte, 4096> buffer{};
    for(std::size_t got = reader.read(buffer); got != 0; got = reader.read(buffer))
        text.append(reinterpret_cast<const char *>(buffer.data()), got);
    return text;
}

}

// The asset_scanner concept hands scanners a const resolved_asset, but draining a
// byte_reader mutates its pull stream; the const_cast is well-defined because the
// closure owns the asset as a non-const local and only narrows it to a const view.
inline std::string read_asset_text(const resolved_asset &asset, log_sink &log)
{
    if(asset.holds_path())
        return detail::read_path_text(asset.path(), log);
    byte_reader &reader = const_cast<resolved_asset &>(asset).bytes();
    if(!reader.valid())
    {
        log.log(level::warn, "asset byte stream is not readable");
        return {};
    }
    return detail::drain_bytes(reader);
}

}

#endif
