#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

expansion_result expand_doc(const std::string &body)
{
    meios::log_sink silent;
    meios::source_stack sources{};
    meios::eval_scope scope;
    const std::string source = R"(<robot name="p" xmlns:xacro="http://www.ros.org/wiki/xacro">)" + body + "</robot>";
    return meios::expand(source, scope, sources, "inline.xacro", meios::expansion_limits{}, silent);
}

std::size_t occurrences(const std::string &text, const std::string &needle)
{
    std::size_t count = 0;
    for(std::size_t at = text.find(needle); at != std::string::npos; at = text.find(needle, at + 1))
        ++count;
    return count;
}

const char *defines_inner = R"(<xacro:macro name="outer"><xacro:macro name="inner"><link name="i"/></xacro:macro>)";

}

TEST_CASE("a macro defined inside a call's body is unknown after the call", "[xacro][macro][call]")
{
    const expansion_result out = expand_doc(std::string(defines_inner) + "</xacro:macro><xacro:outer/><xacro:inner/>");
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::xacro_structural_error);
    REQUIRE(out.error().message.find("<xacro:inner>") != std::string::npos);
}

TEST_CASE("a macro defined inside a call's body is callable later in the same body", "[xacro][macro][call]")
{
    const expansion_result out = expand_doc(std::string(defines_inner) + "<xacro:inner/></xacro:macro><xacro:outer/>");
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("name=\"i\"") != std::string::npos);
}

TEST_CASE("a nested call reaches a macro its caller's body defined before it", "[xacro][macro][call]")
{
    const expansion_result out =
        expand_doc(R"(<xacro:macro name="mid"><xacro:helper/></xacro:macro>)"
                   R"(<xacro:macro name="outer"><xacro:macro name="helper"><link name="h"/></xacro:macro><xacro:mid/></xacro:macro>)"
                   "<xacro:outer/>");
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("name=\"h\"") != std::string::npos);
}

TEST_CASE("a body redefining its own macro finishes the running call and leaves the outer definition in place",
          "[xacro][macro][call]")
{
    const expansion_result out =
        expand_doc(R"(<xacro:macro name="m"><link name="before"/><xacro:macro name="m"><link name="inner"/></xacro:macro>)"
                   R"(<link name="after"/><xacro:m/></xacro:macro><xacro:m/><xacro:m/>)");
    REQUIRE(out.has_value());
    REQUIRE(occurrences(out->document, "name=\"before\"") == 2);
    REQUIRE(occurrences(out->document, "name=\"after\"") == 2);
    REQUIRE(occurrences(out->document, "name=\"inner\"") == 2);
}

TEST_CASE("a call's own child redefining the macro does not reach the running call", "[xacro][macro][call]")
{
    const expansion_result out =
        expand_doc(R"(<xacro:macro name="m" params="a"><link name="old_${a}"/></xacro:macro>)"
                   R"(<xacro:m a="x"><xacro:macro name="m" params="a"><link name="new_${a}"/></xacro:macro></xacro:m>)"
                   R"(<xacro:m a="y"/>)");
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("name=\"old_x\"") != std::string::npos);
    REQUIRE(out->document.find("name=\"new_y\"") != std::string::npos);
    REQUIRE(out->document.find("name=\"new_x\"") == std::string::npos);
}
