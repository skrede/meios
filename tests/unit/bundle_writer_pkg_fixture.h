#ifndef HPP_GUARD_MEIOS_UNIT_BUNDLE_WRITER_PKG_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_BUNDLE_WRITER_PKG_FIXTURE_H

#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace
{

inline std::filesystem::path scratch_root(const std::string &suffix)
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / ("meios_pkg_" + suffix);
    std::filesystem::remove_all(root);
    return root;
}

inline std::filesystem::path write_file(const std::filesystem::path &path, const std::string &text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
    return path;
}

inline std::string read_file(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

}

#endif
