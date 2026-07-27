#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>
#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <algorithm>
#include <filesystem>

static_assert(meios::cleared_by(meios::diagnostic_code::extension_ignored)
              == meios::completeness::none);

namespace
{

struct note
{
    meios::level           lvl;
    meios::diagnostic_code code;
    meios::source_location loc;
    std::string            message;
};

struct recorder
{
    std::vector<note> &notes;

    void operator()(meios::level lvl, const std::string &message)
    {
        notes.push_back({ lvl, meios::diagnostic_code::unspecified, {}, message });
    }

    void operator()(meios::level lvl, const meios::source_location &loc, const std::string &message)
    {
        notes.push_back({ lvl, meios::diagnostic_code::unspecified, loc, message });
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &loc,
                    const std::string &message)
    {
        notes.push_back({ lvl, code, loc, message });
    }
};

std::size_t naming(const std::vector<note> &notes, meios::diagnostic_code code,
                   const std::string &needle)
{
    return static_cast<std::size_t>(
        std::count_if(notes.begin(), notes.end(), [code, &needle](const note &n) {
            return n.code == code && n.message.find(needle) != std::string::npos
                && !n.loc.file.empty() && n.loc.line > 0;
        }));
}

std::size_t errors(const std::vector<note> &notes)
{
    return static_cast<std::size_t>(std::count_if(
        notes.begin(), notes.end(), [](const note &n) { return n.lvl == meios::level::error; }));
}

meios::expected<meios::load_result, meios::load_error> load_fixture(const std::string &name,
                                                                    meios::log_sink &log,
                                                                    meios::strictness strict)
{
    meios::load_options opts;
    opts.strict = strict;
    return meios::load(std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / "profile" / name, opts,
                       log);
}

std::vector<note> parse_text(const std::string &text)
{
    std::vector<note> notes;
    meios::source_stack sources{};
    meios::core_evaluator eval;
    meios::log_sink_f capture{ recorder{ notes } };
    meios::parse_context ctx{ sources,
                              eval,
                              capture,
                              meios::missing_asset::warn,
                              meios::topology_policy::fail,
                              meios::material_policy::warn,
                              meios::strictness::fail,
                              "generated.urdf" };
    meios::pod_recorder<meios::tree<double>> rec(capture, meios::topology_policy::fail);
    meios::basic_parser<meios::urdf_reader> parser(ctx);
    parser.parse(text, rec);
    return notes;
}

}

TEST_CASE("an unrecognized child of the robot element is named and never reaches the model",
          "[urdf][vocabulary]")
{
    std::vector<note> notes;
    meios::log_sink_f capture{ recorder{ notes } };
    const meios::expected<meios::load_result, meios::load_error> loaded =
        load_fixture("unknown_element_robot.urdf", capture, meios::strictness::fail);

    REQUIRE(loaded);
    REQUIRE(naming(notes, meios::diagnostic_code::unknown_element, "telemetry") == 1);
    REQUIRE(loaded->robot.links.size() == 2);
    REQUIRE(loaded->robot.joints.size() == 1);
    REQUIRE_FALSE(meios::has(loaded->claims, meios::completeness::parsed));
}

TEST_CASE("an unrecognized child of a link is named, a level that used to be silent",
          "[urdf][vocabulary]")
{
    std::vector<note> notes;
    meios::log_sink_f capture{ recorder{ notes } };
    const meios::expected<meios::load_result, meios::load_error> loaded =
        load_fixture("unknown_element_link.urdf", capture, meios::strictness::fail);

    REQUIRE(loaded);
    REQUIRE(naming(notes, meios::diagnostic_code::unknown_element, "collision_checking") == 1);
    REQUIRE(loaded->robot.links.at(0).visuals.empty());
    REQUIRE(loaded->robot.links.at(0).collisions.empty());
}

TEST_CASE("an unrecognized child of a joint is named, the other level that used to be silent",
          "[urdf][vocabulary]")
{
    std::vector<note> notes;
    meios::log_sink_f capture{ recorder{ notes } };
    const meios::expected<meios::load_result, meios::load_error> loaded =
        load_fixture("unknown_element_joint.urdf", capture, meios::strictness::fail);

    REQUIRE(loaded);
    REQUIRE(naming(notes, meios::diagnostic_code::unknown_element, "hardware_interface") == 1);
    REQUIRE_FALSE(loaded->robot.joints.at(0).limits.has_value());
}

TEST_CASE("a misspelled joint attribute is named rather than left to its downstream effect",
          "[urdf][vocabulary]")
{
    std::vector<note> notes;
    meios::log_sink_f capture{ recorder{ notes } };
    const meios::expected<meios::load_result, meios::load_error> loaded =
        load_fixture("unknown_attribute_joint.urdf", capture, meios::strictness::fail);

    REQUIRE(loaded);
    REQUIRE(errors(notes) == 0);
    REQUIRE(naming(notes, meios::diagnostic_code::unknown_attribute, "tpye") == 1);
    REQUIRE(loaded->robot.joints.at(0).kind == meios::joint_kind::fixed);
}

TEST_CASE("each recognized extension block is disclosed under its own code", "[urdf][vocabulary]")
{
    for(const std::string tag : { "gazebo", "ros2_control", "transmission", "sensor" })
    {
        INFO(tag);
        const std::vector<note> notes =
            parse_text("<?xml version=\"1.0\"?>\n<robot name=\"extended\">\n"
                       "  <link name=\"base_link\"/>\n  <"
                       + tag + " name=\"block\"/>\n</robot>\n");

        REQUIRE(naming(notes, meios::diagnostic_code::extension_ignored, tag) == 1);
        REQUIRE(naming(notes, meios::diagnostic_code::unknown_element, tag) == 0);
        REQUIRE(errors(notes) == 0);
    }
}

TEST_CASE("dropping unrecognized content is legal at every document-validity setting",
          "[urdf][vocabulary]")
{
    for(meios::strictness strict :
        { meios::strictness::fail, meios::strictness::warn, meios::strictness::skip })
    {
        std::vector<note> notes;
        meios::log_sink_f capture{ recorder{ notes } };
        const meios::expected<meios::load_result, meios::load_error> loaded =
            load_fixture("unknown_element_robot.urdf", capture, strict);

        REQUIRE(loaded);
        REQUIRE(errors(notes) == 0);
        REQUIRE(naming(notes, meios::diagnostic_code::unknown_element, "telemetry") == 1);
    }
}
