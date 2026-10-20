#ifndef HPP_GUARD_MEIOS_UNIT_LOAD_BESIDE_ROOT_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_LOAD_BESIDE_ROOT_FIXTURE_H

#include "xacro_include_tree.h"

#include <meios/urdf.h>

#include <string>
#include <filesystem>
#include <system_error>

namespace beside_root_fixture
{

using load_outcome = meios::expected<meios::load_result, meios::load_error>;

inline meios::load_options rooted(const include_tree::tree &held, bool follow)
{
    meios::load_options opts;
    opts.package_roots                 = {held.root() / "root"};
    opts.includes.follow_outside_links = follow;
    return opts;
}

// doc/sub links to the directory d below parent, so the spelling sub/.. names parent where the
// operating system resolves it and doc where it is read lexically.
inline bool linked_sub(const include_tree::tree &held, const std::string &parent)
{
    std::filesystem::create_directories(held.root() / parent / "d");
    std::filesystem::create_directories(held.root() / "doc");
    std::error_code ec;
    std::filesystem::create_directory_symlink(held.root() / parent / "d", held.root() / "doc" / "sub", ec);
    return !ec;
}

inline std::filesystem::path yaml_document(const include_tree::tree &held, const std::string &at, const std::string &spec)
{
    return held.write(at, include_tree::robot("<link name=\"${xacro.load_yaml('" + spec + "')['name']}\"/>"));
}

inline bool loads_link(const load_outcome &loaded, const std::string &name)
{
    return loaded.has_value() && loaded->robot.links.size() == 1 && loaded->robot.links[0].name == name;
}

}

#endif
