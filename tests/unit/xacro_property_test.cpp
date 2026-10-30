#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <fstream>
#include <utility>
#include <iterator>
#include <filesystem>

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

expansion_result expand_body(const std::string &body, meios::log_sink &log)
{
    meios::source_stack sources{};
    meios::eval_scope scope;
    const std::string source =
        R"(<robot name="r" xmlns:xacro="http://www.ros.org/wiki/xacro">)" + body + "</robot>";
    return meios::expand(source, scope, sources, "inline.xacro", meios::expansion_limits{}, log);
}

// The minimized document a behavior was measured on against the reference, read from where the
// differential left it once the two sides agreed and it named no divergence.
expansion_result expand_probe(const char *name, meios::log_sink &log)
{
    std::ifstream in(std::filesystem::path{ MEIOS_XACRO_PROBE_DIR } / name, std::ios::binary);
    std::string source;
    source.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    meios::source_stack sources{};
    meios::eval_scope scope;
    return meios::expand(source, scope, sources, "probe.xacro", meios::expansion_limits{}, log);
}

}

TEST_CASE("a property's default attribute binds an unbound name", "[xacro][scope][property]")
{
    meios::log_sink silent;
    const expansion_result out = expand_body(
        R"(<xacro:arg name="a" default="from_arg"/>)"
        R"x(<xacro:property name="p" default="$(arg a)"/><link name="${p}"/>)x", silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find(R"(name="from_arg")") != std::string::npos);
}

TEST_CASE("a property's default attribute leaves an already-bound name alone",
          "[xacro][scope][property]")
{
    meios::log_sink silent;
    const expansion_result out = expand_body(
        R"(<xacro:property name="p" value="first"/>)"
        R"(<xacro:property name="p" default="second"/><link name="${p}"/>)", silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find(R"(name="first")") != std::string::npos);
}

TEST_CASE("a property writing both a value and a default attribute refuses",
          "[xacro][scope][property]")
{
    meios::log_sink silent;
    const expansion_result out = expand_body(
        R"(<xacro:property name="p" value="v" default="d"/><link name="${p}"/>)", silent);
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::xacro_structural_error);
    REQUIRE(out.error().message.find("value") != std::string::npos);
    REQUIRE(out.error().message.find("default") != std::string::npos);
    REQUIRE(out.error().loc.line == 1);
}

TEST_CASE("a property whose default attribute is empty binds the empty string",
          "[xacro][scope][property]")
{
    meios::log_sink silent;
    const expansion_result out =
        expand_body(R"(<xacro:property name="p" default=""/><link name="x${p}y"/>)", silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find(R"(name="xy")") != std::string::npos);
}

// Measured rather than chosen: upstream turns a property carrying neither attribute into a
// block-valued name no expression can read, and this side binds the empty string. Nothing in
// this file changes that; the case records what the binding is so a later change to it is
// visible rather than silent.
TEST_CASE("a property writing neither attribute still binds the empty string",
          "[xacro][scope][property]")
{
    meios::log_sink silent;
    const expansion_result out =
        expand_body(R"(<xacro:property name="p"/><link name="x${p}y"/>)", silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find(R"(name="xy")") != std::string::npos);
}

TEST_CASE("the minimized fallback-attribute document renders the argument's text",
          "[xacro][scope][property]")
{
    meios::log_sink silent;
    const expansion_result out = expand_probe("property_fallback.xacro", silent);
    REQUIRE(out.has_value());
    REQUIRE(meios::canonical_xml(out->document)
            == R"(<robot name="probe"><link name="from_arg"></link></robot>)");
}

// Each expected render was measured on the same document against xacro 2.1.1.
TEST_CASE("the minimized binding documents render as the reference renders them", "[xacro][scope][property]")
{
    const std::pair<const char *, const char *> rows[] = {
            {"property_default_over_argument.xacro", "Bbase_Aarm"}, {"property_value_over_argument.xacro", "B_A"},      {"property_before_argument.xacro", "B_A"},
            {"global_property_over_argument.xacro", "G_A"},         {"argument_beside_parameter.xacro", "argv_base"},   {"property_default_under_parameter.xacro", "mine_base"},
            {"property_default_reads_argument.xacro", "from_arg"},  {"inherited_default_skips_argument.xacro", "Zarm"},
    };
    for(const std::pair<const char *, const char *> &row : rows)
    {
        DYNAMIC_SECTION(row.first)
        {
            meios::log_sink silent;
            const expansion_result out = expand_probe(row.first, silent);
            REQUIRE(out.has_value());
            REQUIRE(meios::canonical_xml(out->document) == std::string(R"(<robot name="probe"><link name=")") + row.second + R"("></link></robot>)");
        }
    }
}

TEST_CASE("a read of an argument other than through the argument command refuses and names it", "[xacro][scope][property]")
{
    const std::pair<const char *, meios::diagnostic_code> rows[] = {
            {"argument_by_bare_name.xacro", meios::diagnostic_code::undefined_property},
            {"argument_command_reads_no_property.xacro", meios::diagnostic_code::unresolved_arg},
            {"inherited_argument_refuses.xacro", meios::diagnostic_code::xacro_structural_error},
    };
    for(const std::pair<const char *, meios::diagnostic_code> &row : rows)
    {
        DYNAMIC_SECTION(row.first)
        {
            meios::log_sink silent;
            const expansion_result out = expand_probe(row.first, silent);
            REQUIRE_FALSE(out.has_value());
            REQUIRE(out.error().code == row.second);
            REQUIRE(out.error().message.find("$(arg p)") != std::string::npos);
        }
    }
}

TEST_CASE("a property's lazy_eval read as a boolean leaves the binding as it was", "[xacro][scope][property]")
{
    for(const char *spelling : {"false", "0", "${False}", "true", ""})
    {
        DYNAMIC_SECTION(spelling)
        {
            meios::log_sink silent;
            const expansion_result out = expand_body(std::string(R"(<xacro:property name="p" value="v" lazy_eval=")") + spelling + R"("/><link name="${p}"/>)", silent);
            REQUIRE(out.has_value());
            REQUIRE(out->document.find(R"(name="v")") != std::string::npos);
        }
    }
}

TEST_CASE("a property's lazy_eval outside the boolean rule refuses as the reference does", "[xacro][scope][property]")
{
    for(const char *spelling : {"yes", "1.0"})
    {
        DYNAMIC_SECTION(spelling)
        {
            meios::log_sink silent;
            const expansion_result out = expand_body(std::string(R"(<xacro:property name="p" value="v" lazy_eval=")") + spelling + R"("/><link name="${p}"/>)", silent);
            REQUIRE_FALSE(out.has_value());
            REQUIRE(out.error().code == meios::diagnostic_code::xacro_structural_error);
            REQUIRE(out.error().message == std::string("Xacro conditional \"") + spelling + "\" evaluated to \"" + spelling + "\", which is not a boolean expression.");
        }
    }
}
