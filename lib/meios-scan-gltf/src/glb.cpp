#include "glb.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <string>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace meios::detail
{

namespace
{

constexpr std::uint32_t glb_magic = 0x46546C67;
constexpr std::uint32_t chunk_json = 0x4E4F534A;

std::uint32_t read_u32(std::string_view bytes, std::size_t offset)
{
    const auto byte = [&](std::size_t i) { return static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset + i])); };
    return byte(0) | (byte(1) << 8) | (byte(2) << 16) | (byte(3) << 24);
}

}

bool looks_like_glb(std::string_view bytes)
{
    return bytes.size() >= 4 && read_u32(bytes, 0) == glb_magic;
}

std::optional<std::string> extract_glb_json(std::string_view bytes, log_sink &log)
{
    if(bytes.size() < 20 || read_u32(bytes, 0) != glb_magic)
    {
        log.log(level::error, "glb: truncated or bad magic header");
        return {};
    }
    if(read_u32(bytes, 4) != 2)
    {
        log.log(level::error, "glb: unsupported container version (expected 2)");
        return {};
    }
    if(read_u32(bytes, 8) != bytes.size())
    {
        log.log(level::error, "glb: declared length does not match the actual byte size");
        return {};
    }
    const std::uint32_t json_len = read_u32(bytes, 12);
    if(read_u32(bytes, 16) != chunk_json || std::uint64_t{ 20 } + json_len > bytes.size())
    {
        log.log(level::error, "glb: first chunk is not a bounded JSON chunk");
        return {};
    }
    return std::string(bytes.substr(20, json_len));
}

}
