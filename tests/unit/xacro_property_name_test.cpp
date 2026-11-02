#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <fstream>
#include <iterator>
#include <filesystem>
#include <string_view>

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

expansion_result expand_text(const std::string &source)
{
    meios::log_sink silent;
    meios::source_stack sources{};
    meios::eval_scope scope;
    return meios::expand(source, scope, sources, "inline.xacro", meios::expansion_limits{}, silent);
}

expansion_result expand_body(std::string_view body)
{
    return expand_text(R"(<robot name="r" xmlns:xacro="http://www.ros.org/wiki/xacro">)" + std::string(body) + "</robot>");
}

std::string read_probe(const char *name)
{
    std::ifstream in(std::filesystem::path{MEIOS_XACRO_PROBE_DIR} / name, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

bool renders(const expansion_result &out, std::string_view fragment)
{
    return out.has_value() && out->document.find(fragment) != std::string::npos;
}

bool refuses_with(const expansion_result &out, meios::diagnostic_code code, std::string_view message)
{
    return !out.has_value() && out.error().code == code && out.error().message == message;
}

}

TEST_CASE("a property named by an expression binds the name it evaluates to", "[xacro][scope][property]")
{
    const expansion_result out = expand_body(R"(<xacro:property name="p" value="a"/><xacro:property name="${p}_x" value="1"/><link name="l${a_x}"/>)");
    REQUIRE(renders(out, R"(<link name="l1"/>)"));
}

TEST_CASE("a property named by the value another property holds binds under that value", "[xacro][scope][property]")
{
    REQUIRE(renders(expand_text(read_probe("property_name_from_expression.xacro")), R"(<link name="l7"/>)"));
}

TEST_CASE("a property whose name evaluates to something other than text refuses", "[xacro][scope][property]")
{
    const expansion_result out = expand_body(R"(<xacro:property name="${1}" value="1"/><link name="l"/>)");
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::xacro_structural_error);
    REQUIRE(out.error().message.find("must be named by text") != std::string::npos);
}

TEST_CASE("a property name that is not a python identifier refuses as the reference does", "[xacro][scope][property]")
{
    for(const char *name : {"1", "for", "True", "a b"})
    {
        DYNAMIC_SECTION(name)
        {
            const expansion_result out = expand_body(std::string(R"(<xacro:property name=")") + name + R"(" value="1"/><link name="l"/>)");
            REQUIRE(refuses_with(out, meios::diagnostic_code::xacro_structural_error, std::string("Property names must be valid python identifiers: ") + name));
        }
    }
}

TEST_CASE("a property name opening with a double underscore refuses before remove is read", "[xacro][scope][property]")
{
    for(const char *remove : {"true", "${nope}"})
    {
        DYNAMIC_SECTION(remove)
        {
            const expansion_result out = expand_body(std::string(R"(<xacro:property name="__x" remove=")") + remove + R"("/><link name="l"/>)");
            REQUIRE(refuses_with(out, meios::diagnostic_code::xacro_structural_error, "Property names must not start with double underscore: __x"));
        }
    }
    const expansion_result out = expand_body(R"(<xacro:property name="__π" value="1"/><link name="l"/>)");
    REQUIRE(refuses_with(out, meios::diagnostic_code::xacro_structural_error, "Property names must not start with double underscore: __π"));
}

TEST_CASE("a non-ASCII, underscored or soft-keyword property name the reference admits binds", "[xacro][scope][property]")
{
    const expansion_result out = expand_body(R"(<xacro:property name="π" value="1"/><xacro:property name="a·" value="4"/>)"
                                             R"(<xacro:property name="Ω" value="5"/><xacro:property name="x́" value="6"/><xacro:property name="é" value="7"/>)"
                                             R"(<xacro:property name="_m" value="2"/><xacro:property name="match" value="3"/><link name="l${_m}${match}"/>)");
    REQUIRE(renders(out, R"(<link name="l23"/>)"));
}

TEST_CASE("a property name that is not a python identifier outside ASCII refuses as the reference does", "[xacro][scope][property]")
{
    for(const char *name : {"a€", "＿x", "·a", "ﬁ", "ａ", "é", "Ω", "_＿x"})
    {
        DYNAMIC_SECTION(name)
        {
            const expansion_result out = expand_body(std::string(R"(<xacro:property name=")") + name + R"(" value="1"/><link name="l"/>)");
            REQUIRE(refuses_with(out, meios::diagnostic_code::xacro_structural_error, std::string("Property names must be valid python identifiers: ") + name));
        }
    }
}

TEST_CASE("a property without a name attribute refuses before anything else about it is read", "[xacro][scope][property]")
{
    const expansion_result out = expand_body(R"(<xacro:property value="1" remove="${nope}"/><link name="l"/>)");
    REQUIRE(refuses_with(out, meios::diagnostic_code::xacro_structural_error, "xacro:property: missing attribute 'name'"));
}
