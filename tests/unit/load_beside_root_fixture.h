#ifndef HPP_GUARD_MEIOS_UNIT_LOAD_BESIDE_ROOT_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_LOAD_BESIDE_ROOT_FIXTURE_H

#include "xacro_include_tree.h"

#include <meios/urdf.h>

#include <string>
#include <variant>
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

// doc/sub links to the directory d below parent. The spelling sub/.. names parent where the operating system follows
// the link before reducing .., as POSIX does, and doc where it reduces .. first, as Windows does.
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

inline std::filesystem::path mesh_document(const include_tree::tree &held, const std::string &filename)
{
    return held.write("doc/robot.xacro", include_tree::robot("<link name=\"base\"><visual><geometry><mesh filename=\"" + filename + "\"/></geometry></visual></link>"));
}

inline bool resolves_to(const load_outcome &loaded, const std::filesystem::path &expected)
{
    if(!loaded.has_value() || loaded->robot.links.size() != 1 || loaded->robot.links[0].visuals.size() != 1)
        return false;
    const auto *mesh = std::get_if<meios::mesh<>>(&loaded->robot.links[0].visuals[0].geom.shape);
    std::error_code error;
    return mesh != nullptr && mesh->resolved_path && std::filesystem::equivalent(*mesh->resolved_path, expected, error);
}

}

#endif
