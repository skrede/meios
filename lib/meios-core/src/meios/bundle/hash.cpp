#include "bundle_detail.h"

#include <string>
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace meios::detail
{

namespace
{

std::uint32_t fnv1a_32(std::string_view bytes)
{
    std::uint32_t hash = 2166136261u;
    for(char byte : bytes)
    {
        hash ^= static_cast<unsigned char>(byte);
        hash *= 16777619u;
    }
    return hash;
}

std::string to_hex8(std::uint32_t value)
{
    std::string out(8, '0');
    for(int i = 7; i >= 0; --i)
    {
        out[static_cast<std::size_t>(i)] = "0123456789abcdef"[value & 0xFu];
        value >>= 4;
    }
    return out;
}

}

std::string normal_root(const std::filesystem::path &root)
{
    std::error_code ec;
    const std::filesystem::path canonical = std::filesystem::weakly_canonical(root, ec);
    return (ec ? root.lexically_normal() : canonical).generic_string();
}

std::string collision_suffix(const std::filesystem::path &root)
{
    return to_hex8(fnv1a_32(normal_root(root)));
}

}
