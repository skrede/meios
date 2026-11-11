#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

const std::string no_break_space    = "\xC2\xA0";
const std::string ideographic_space = "\xE3\x80\x80";

expansion_result expand_macro(const std::string &params, const std::string &call, const std::string &body)
{
    meios::log_sink silent;
    meios::source_stack sources{};
    meios::eval_scope scope;
    const std::string macro  = "<xacro:macro name=\"m\" params=\"" + params + "\">" + body + "</xacro:macro>";
    const std::string source = "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">" + macro + call + "</robot>";
    return meios::expand(source, scope, sources, "inline.xacro", meios::expansion_limits{}, silent);
}

void require_link(const expansion_result &out, const std::string &name)
{
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("<link name=\"" + name + "\"") != std::string::npos);
}

}

TEST_CASE("a no-break space separates two macro parameters with defaults", "[xacro][structural][macro]")
{
    require_link(expand_macro("a:=1" + no_break_space + "b:=2", "<xacro:m/>", "<link name=\"l_${a}_${b}\"/>"), "l_1_2");
}

TEST_CASE("an ideographic space separates two macro parameters the call binds", "[xacro][structural][macro]")
{
    require_link(expand_macro("a" + ideographic_space + "b", "<xacro:m a=\"x\" b=\"y\"/>", "<link name=\"${a}${b}\"/>"), "xy");
}

TEST_CASE("a form feed separates two macro parameters", "[xacro][structural][macro]")
{
    require_link(expand_macro("a\fb", "<xacro:m a=\"x\" b=\"y\"/>", "<link name=\"${a}${b}\"/>"), "xy");
}

TEST_CASE("a params attribute holding only a no-break space refuses at the definition", "[xacro][structural][macro]")
{
    const expansion_result out = expand_macro(no_break_space, "", "<link name=\"l\"/>");
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::xacro_structural_error);
    REQUIRE(out.error().message.find("'m'") != std::string::npos);
}

TEST_CASE("a no-break space inside a quoted default stays part of the default", "[xacro][structural][macro]")
{
    require_link(expand_macro("a:='x" + no_break_space + "y'", "<xacro:m/>", "<link name=\"l_${a}\"/>"), "l_x" + no_break_space + "y");
}

TEST_CASE("a truncated UTF-8 sequence ending a params attribute stays part of the last default", "[xacro][structural][macro]")
{
    for(const std::string truncated : {"\xC2", "\xE3\x80"})
        require_link(expand_macro("a:=1" + truncated, "<xacro:m/>", "<link name=\"l_${a}\"/>"), "l_1" + truncated);
}
