#ifndef HPP_GUARD_MEIOS_TEST_ROS_PACKAGE_FIXTURE_H
#define HPP_GUARD_MEIOS_TEST_ROS_PACKAGE_FIXTURE_H

#include "source_lookup_fixture.h"

#include <meios/io/resolved_asset.h>
#include <meios/io/directory_source.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <string_view>

namespace ros_test
{

constexpr std::string_view mesh = "meshes/x.stl";

inline void write_file(const std::filesystem::path &path, std::string_view content)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path) << content;
}

// Both seeders answer with the base the discovery layer will register for the package, so a
// case compares authorization against the layout it wrote rather than against a returned path.
inline std::filesystem::path ament_package(const std::filesystem::path &prefix, std::string_view pkg)
{
    write_file(prefix / "share" / "ament_index" / "resource_index" / "packages" / pkg, "");
    write_file(prefix / "share" / pkg / mesh, "solid\n");
    return meios::detail::absolute_base(prefix) / "share" / pkg;
}

inline std::filesystem::path ros1_package(const std::filesystem::path &dir, std::string_view name)
{
    write_file(dir / "package.xml", "<package><name>" + std::string(name) + "</name></package>");
    write_file(dir / mesh, "solid\n");
    return acquisition_test::real_path(dir);
}

inline bool has_package(const std::vector<std::string> &names, std::string_view wanted)
{
    return std::find(names.begin(), names.end(), std::string(wanted)) != names.end();
}

// A hit authorizes a later checked open with the base the discovery layer found and the
// caller's own relative half, neither of them recovered from the presentation path.
inline void require_authorized(const std::optional<meios::resolved_asset> &hit, const std::filesystem::path &base)
{
    REQUIRE(hit.has_value());
    REQUIRE(hit->path().is_absolute());
    REQUIRE(std::filesystem::exists(hit->path()));
    REQUIRE(hit->source_root() == std::optional{base});
    REQUIRE(hit->source_relative() == std::optional{std::filesystem::path(mesh)});
    REQUIRE(acquisition_test::real_path(base / mesh) == acquisition_test::real_path(hit->path()));
}

}

#endif
