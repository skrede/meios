#include <meios/urdf.h>
#include <meios/model.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <filesystem>

namespace
{

std::filesystem::path fixture(const std::string &name)
{
    return std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / name;
}

}

TEST_CASE("a duplicate attribute propagates through load()'s return under the silent default",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("dup_attr.urdf"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::duplicate_attribute);
}

TEST_CASE("trailing content propagates through load()'s return under the silent default",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("trailing_garbage.urdf"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::trailing_content);
}

TEST_CASE("an undefined property under eval_policy::fail returns a located error", "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.eval = meios::eval_policy::fail;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("undefined_property.xacro"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE_FALSE(result.error().loc.file.empty());
}

TEST_CASE("an unresolvable $(find) include under eval_policy::fail returns a located error",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.eval = meios::eval_policy::fail;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("unresolvable_find_include.xacro"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE_FALSE(result.error().loc.file.empty());
}

TEST_CASE("an undefined material under material_policy::fail propagates the specific code",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.materials = meios::material_policy::fail;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("unresolved_material.urdf"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::undefined_material);
}

TEST_CASE("an unresolved mesh under missing_asset::fail propagates the specific code",
          "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.on_missing = meios::missing_asset::fail;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("package_mesh.urdf"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::unresolved_mesh);
}

TEST_CASE("a warn material policy still returns a populated model", "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.materials = meios::material_policy::warn;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("unresolved_material.urdf"), opts);

    REQUIRE(result.has_value());
}

TEST_CASE("a warn missing-asset policy still returns a populated model", "[urdf][load_failure]")
{
    meios::load_options opts;
    opts.on_missing = meios::missing_asset::warn;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("package_mesh.urdf"), opts);

    REQUIRE(result.has_value());
}
