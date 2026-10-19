#ifndef HPP_GUARD_MEIOS_UNIT_LINKED_INSTALL_TREE_H
#define HPP_GUARD_MEIOS_UNIT_LINKED_INSTALL_TREE_H

#include "xacro_include_tree.h"

#include <string>
#include <vector>
#include <utility>
#include <filesystem>
#include <system_error>

namespace linked_install
{

using files = std::vector<std::pair<std::string, std::string>>;

inline void write_sources(const include_tree::tree &held, const files &written)
{
    for(const std::pair<std::string, std::string> &file : written)
        held.write("src/urdf/" + file.first, file.second);
}

inline std::filesystem::path install_dir(const include_tree::tree &held)
{
    return held.root() / "install" / "urdf";
}

inline std::filesystem::path linked_directory(const include_tree::tree &held, const files &written, const std::string &document)
{
    write_sources(held, written);
    std::filesystem::create_directories(held.root() / "install");
    std::error_code ec;
    std::filesystem::create_directory_symlink(held.root() / "src" / "urdf", install_dir(held), ec);
    return ec ? std::filesystem::path{} : install_dir(held) / document;
}

inline std::filesystem::path per_file_links(const include_tree::tree &held, const files &written, const std::string &document)
{
    write_sources(held, written);
    std::filesystem::create_directories(install_dir(held));
    for(const std::pair<std::string, std::string> &file : written)
        if(!include_tree::linked(held.root() / "src" / "urdf" / file.first, install_dir(held) / file.first))
            return {};
    return install_dir(held) / document;
}

inline std::filesystem::path link_elsewhere(const include_tree::tree &held, const std::string &name, const std::filesystem::path &target)
{
    std::filesystem::create_directories(install_dir(held));
    const std::filesystem::path link = install_dir(held) / name;
    return include_tree::linked(target, link) ? link : std::filesystem::path{};
}

}

#endif
