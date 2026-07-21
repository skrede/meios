#include "meios/io/materialize.h"

#include "meios/diagnostic/level.h"

#include <span>
#include <array>
#include <string>
#include <random>
#include <utility>
#include <cstddef>
#include <fstream>
#include <filesystem>
#include <system_error>

namespace meios
{

namespace
{

// A non-predictable 128-bit stem drawn from the platform entropy source; the
// unpredictable name is the control against symlink pre-creation on the temp
// path, and combined with the exists() check avoids clobbering a live file.
std::string random_stem()
{
    static const char digits[] = "0123456789abcdef";
    std::random_device device;
    std::string stem = "meios-";
    for(int i = 0; i < 16; ++i)
    {
        unsigned value = device() & 0xffu;
        stem.push_back(digits[value >> 4]);
        stem.push_back(digits[value & 0xfu]);
    }
    return stem;
}

std::filesystem::path unique_temp_path()
{
    std::filesystem::path directory = std::filesystem::temp_directory_path();
    std::error_code ec;
    for(;;)
    {
        std::filesystem::path candidate = directory / random_stem();
        if(!std::filesystem::exists(candidate, ec))
            return candidate;
    }
}

void drain(byte_reader &reader, std::ofstream &out)
{
    std::array<std::byte, 4096> buffer{};
    for(std::size_t n = reader.read(buffer); n != 0; n = reader.read(buffer))
        out.write(reinterpret_cast<const char *>(buffer.data()),
                  static_cast<std::streamsize>(n));
}

}

resolved_asset materialize(resolved_asset &&asset, log_sink &log)
{
    if(asset.holds_path())
        return std::move(asset);

    std::filesystem::path path = unique_temp_path();
    std::ofstream out(path, std::ios::binary | std::ios::out);
    drain(asset.bytes(), out);
    out.close();

    std::error_code ec;
    std::filesystem::path resolved = std::filesystem::weakly_canonical(path, ec);
    std::filesystem::path canonical = ec ? path : resolved;
    log.log(level::info, "materialized bytes asset to temporary file " + canonical.string());
    return resolved_asset{ canonical, temp_file_guard{ canonical } };
}

}
