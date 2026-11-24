#include "bundle_writer_fixture.h"

#include <meios/bundle.h>
#include <meios/model.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <sstream>

TEST_CASE("special characters in a name round-trip through escaping", "[bundle][writer]")
{
    capture_log log;
    std::ostringstream out;
    meios::urdf_writer writer(out, log);
    meios::tree<double> robot;
    robot.name = "gen";
    meios::link<double> arm{};
    arm.name = "arm&<\"x";
    robot.links.push_back(arm);
    meios::replay(robot, writer);
    REQUIRE(writer.status().status == meios::emit_status::ok);

    capture_log relog;
    const meios::tree<double> round = reparse(out.str(), relog);
    REQUIRE(round.links.size() == 1);
    REQUIRE(round.links[0].name == "arm&<\"x");
}

TEST_CASE("an un-encodable control character hard-fails the emitter", "[bundle][writer]")
{
    capture_log log;
    std::ostringstream out;
    meios::urdf_writer writer(out, log);
    meios::tree<double> robot;
    robot.name = "gen";
    meios::link<double> arm{};
    arm.name = std::string("arm") + '\x01';
    robot.links.push_back(arm);
    meios::replay(robot, writer);

    REQUIRE(writer.status().status == meios::emit_status::xml_unencodable);
    REQUIRE(log.saw(meios::level::error));
    REQUIRE(out.str().empty());
}

TEST_CASE("an overlong UTF-8 encoding hard-fails the emitter", "[bundle][writer]")
{
    capture_log log;
    std::ostringstream out;
    meios::urdf_writer writer(out, log);
    meios::tree<double> robot;
    robot.name = "gen";
    meios::link<double> arm{};
    arm.name = std::string("arm") + '\xC0' + '\xBC';
    robot.links.push_back(arm);
    meios::replay(robot, writer);

    REQUIRE(writer.status().status == meios::emit_status::xml_unencodable);
    REQUIRE(log.saw(meios::level::error));
    REQUIRE(out.str().empty());
}

TEST_CASE("an empty name is preserved with a warning, not a failure", "[bundle][writer]")
{
    capture_log log;
    std::ostringstream out;
    meios::urdf_writer writer(out, log);
    meios::tree<double> robot;
    robot.name = "gen";
    meios::link<double> arm{};
    arm.name = "";
    robot.links.push_back(arm);
    meios::replay(robot, writer);

    REQUIRE(writer.status().status == meios::emit_status::ok);
    REQUIRE(log.saw(meios::level::warn));
    REQUIRE_FALSE(out.str().empty());
}
