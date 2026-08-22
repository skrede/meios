#ifndef HPP_GUARD_MEIOS_URDF_LFS_POINTER_H
#define HPP_GUARD_MEIOS_URDF_LFS_POINTER_H

#include <string>
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace meios::detail
{

// An unsmudged Git-LFS object is a tiny UTF-8 text file whose first line is the spec
// version prefix; archives built without a smudge substitute one for each binary mesh.
// https://github.com/git-lfs/git-lfs/blob/main/docs/spec.md
inline bool is_lfs_pointer(const std::filesystem::path &path)
{
    constexpr std::string_view signature = "version https://git-lfs.github.com/spec/";
    std::error_code ec;
    const std::uintmax_t size = std::filesystem::file_size(path, ec);
    if(ec || size == 0 || size > 1024)
        return false;
    std::ifstream in(path, std::ios::binary);
    if(!in)
        return false;
    std::string head(signature.size(), '\0');
    in.read(head.data(), static_cast<std::streamsize>(signature.size()));
    return in.gcount() == static_cast<std::streamsize>(signature.size()) && head == signature;
}

}

#endif
