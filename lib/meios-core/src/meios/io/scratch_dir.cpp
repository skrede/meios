#include "meios/io/scratch_dir.h"

#include <random>
#include <string>
#include <fstream>
#include <optional>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace meios
{

namespace
{

// A non-predictable 128-bit stem drawn from the platform entropy source; the
// unpredictable name is the control against symlink pre-creation on the temp path.
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

// mkdir's mode is 0777 masked by umask, so a fresh directory is world-readable until
// this narrows it; the window is small but real.
bool narrow_to_owner(const std::filesystem::path &directory, std::error_code &ec)
{
    std::filesystem::permissions(directory, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace, ec);
    return !ec;
}

// A candidate that could not be narrowed is removed rather than left behind: an abandoned
// candidate is a directory nothing owns and nothing will delete.
std::optional<std::filesystem::path> remove_and_refuse(const std::filesystem::path &candidate)
{
    std::error_code cleanup;
    std::filesystem::remove_all(candidate, cleanup);
    return std::nullopt;
}

}

// create_directory is specified as-if POSIX mkdir, which fails atomically when the
// path already exists; that is what makes an exclusive create expressible without a
// platform header, C++20 <fstream> carrying no exclusive open mode (std::ios::noreplace
// is C++23, P2467R1). A create reporting neither success nor an error found the name
// taken, which is that same collision spelled without an error_code; every other error
// is permanent for the run and ends the loop with its reason left for the caller.
std::optional<std::filesystem::path> detail::create_scratch_root(
    const std::filesystem::path &parent, std::error_code &ec)
{
    for(int attempt = 0; attempt < 8; ++attempt)
    {
        const std::filesystem::path candidate = parent / random_stem();
        ec.clear();
        if(std::filesystem::create_directory(candidate, ec))
            return narrow_to_owner(candidate, ec) ? std::optional{ candidate }
                                                  : remove_and_refuse(candidate);
        if(ec && ec != std::errc::file_exists)
            return std::nullopt;
    }
    return std::nullopt;
}

bool detail::write_scratch_entry(const std::filesystem::path &path, std::string_view bytes)
{
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if(ec)
        return false;
    std::ofstream out(path, std::ios::binary | std::ios::out | std::ios::trunc);
    if(!out.is_open())
        return false;
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    out.close();
    return !out.fail();
}

}
