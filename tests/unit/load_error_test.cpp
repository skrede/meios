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

TEST_CASE("a cannot-open path is reported through load_error carrying the path", "[urdf][load_error]")
{
    meios::load_options opts;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("does_not_exist.urdf"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().loc.file == fixture("does_not_exist.urdf"));
}

TEST_CASE("a malformed-XML document yields a load_error with a converted line", "[urdf][load_error]")
{
    meios::load_options opts;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("malformed_xml.urdf"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().loc.line > 0);
}

TEST_CASE("a non-<robot> root yields a load_error with a real location", "[urdf][load_error]")
{
    meios::load_options opts;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("not_a_robot.xml"), opts);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().loc.line > 0);
    REQUIRE(result.error().message.find("robot") != std::string::npos);
}

TEST_CASE("a valid description yields a populated model", "[urdf][load_error]")
{
    meios::load_options opts;
    const meios::expected<meios::model<double>, meios::load_error> result =
        meios::load(fixture("test-desc.urdf"), opts);

    REQUIRE(result.has_value());
    REQUIRE(result->name == "test");
    REQUIRE(result->links.size() == 1);
}
