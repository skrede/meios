#include "load_failure_fixture.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
std::filesystem::path make_pkg_root()
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    std::filesystem::path root =
        std::filesystem::temp_directory_path() / ("meios_escape_pkg_" + std::to_string(stamp));
    std::filesystem::create_directories(root / "pkg");
    return root;
}

}

TEST_CASE("a duplicate attribute propagates through load()'s return under the silent default",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("dup_attr.urdf"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::duplicate_attribute);
}

TEST_CASE("trailing content propagates through load()'s return under the silent default",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("trailing_garbage.urdf"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::trailing_content);
}

TEST_CASE("an undefined material under material_policy::fail propagates the specific code",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.materials = meios::material_policy::fail;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("unresolved_material.urdf"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::undefined_material);
}

TEST_CASE("an unresolved mesh under missing_asset::fail propagates the specific code",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.on_missing = meios::missing_asset::fail;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("package_mesh.urdf"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::unresolved_asset);
}

TEST_CASE("a warn material policy still returns a populated model", "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.materials = meios::material_policy::warn;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("unresolved_material.urdf"), opts);

    REQUIRE(result.has_value());
}

TEST_CASE("a warn missing-asset policy still returns a populated model", "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.on_missing = meios::missing_asset::warn;
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("package_mesh.urdf"), opts);

    REQUIRE(result.has_value());
}

TEST_CASE("an over-long include path becomes a typed error, never a filesystem_error abort",
          "[urdf][load_failure]")
{
    // A single include component past the OS name limit makes the throwing
    // filesystem overloads raise ENAMETOOLONG; the load must fail closed with a
    // typed diagnostic instead of terminating the process.
    meios::load_options opts;
    opts.eval = meios::eval_policy::fail;
    meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("overlong_include.xacro"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::unresolved_include);
}

TEST_CASE("a root-escaping package path fails the load under the default policy",
          "[urdf][load_failure]")
{
    const std::filesystem::path root = make_pkg_root();

    meios::load_options opts;
    opts.package_roots = { root };
    const meios::expected<meios::load_result, meios::load_error> result =
        meios::load(fixture("package_escape_mesh.urdf"), opts);

    std::filesystem::remove_all(root);

    REQUIRE_FALSE(result.has_value());
}
