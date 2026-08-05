#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

expansion_result expand_source(const char *source, meios::log_sink &log)
{
    meios::source_stack sources{};
    meios::eval_scope scope;
    return meios::expand(source, scope, sources, "inline.xacro", meios::expansion_limits{}, log);
}

const char *single_quoted_multiword =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:macro name=\"m\" params=\"prefix xyz:='0 0 0'\">"
    "<link name=\"${prefix}\"/><joint name=\"j\"><origin xyz=\"${xyz}\"/></joint>"
    "</xacro:macro><xacro:m prefix=\"a\"/></robot>";

const char *double_quoted_multiword =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:macro name=\"m\" params=\"prefix xyz:=&quot;0 0 0&quot;\">"
    "<link name=\"${prefix}\"/><joint name=\"j\"><origin xyz=\"${xyz}\"/></joint>"
    "</xacro:macro><xacro:m prefix=\"a\"/></robot>";

const char *single_token_quoted =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:macro name=\"m\" params=\"prefix tag:='world'\"><link name=\"${tag}\"/>"
    "</xacro:macro><xacro:m prefix=\"a\"/></robot>";

const char *bare_separator_multiword =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:macro name=\"m\" params=\"prefix xyz='0 0 0'\">"
    "<joint name=\"j\"><origin xyz=\"${xyz}\"/></joint>"
    "</xacro:macro><xacro:m prefix=\"a\"/></robot>";

const char *bare_separator_token =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:macro name=\"m\" params=\"prefix xyz=0\"><link name=\"l${xyz}\"/>"
    "</xacro:macro><xacro:m prefix=\"a\"/></robot>";

const char *fallback_quoted_multiword =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:macro name=\"m\" params=\"prefix xyz:=^|'0 0 0'\">"
    "<joint name=\"j\"><origin xyz=\"${xyz}\"/></joint>"
    "</xacro:macro><xacro:m prefix=\"a\"/></robot>";

const char *unquoted_baseline =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:macro name=\"m\" params=\"prefix tag:=world\"><link name=\"${tag}\"/>"
    "</xacro:macro><xacro:m prefix=\"a\"/></robot>";

const char *block_spellings =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:macro name=\"m\" params=\"*shape **extra\">"
    "<link name=\"l\"><xacro:insert_block name=\"shape\"/></link>"
    "<xacro:insert_block name=\"extra\"/></xacro:macro>"
    "<xacro:m><origin xyz=\"1 2 3\"/><group><mass value=\"1\"/></group></xacro:m></robot>";

const char *unterminated_quote =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:macro name=\"m\" params=\"prefix tag:='world\"><link name=\"${tag}\"/>"
    "</xacro:macro><xacro:m prefix=\"a\"/></robot>";

}

TEST_CASE("a single-quoted multi-word macro default binds its components as one value",
          "[xacro][structural][macro]")
{
    meios::log_sink silent;
    const expansion_result out = expand_source(single_quoted_multiword, silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("xyz=\"0 0 0\"") != std::string::npos);
    REQUIRE(out->document.find("name=\"a\"") != std::string::npos);
}

TEST_CASE("a double-quoted multi-word macro default binds exactly as the single-quoted spelling",
          "[xacro][structural][macro]")
{
    meios::log_sink silent;
    const expansion_result out = expand_source(double_quoted_multiword, silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("xyz=\"0 0 0\"") != std::string::npos);
    REQUIRE(out->document.find("name=\"a\"") != std::string::npos);
}

TEST_CASE("a single-token quoted macro default reaches emitted output without its quotes",
          "[xacro][structural][macro]")
{
    meios::log_sink silent;
    const expansion_result out = expand_source(single_token_quoted, silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("name=\"world\"") != std::string::npos);
    REQUIRE(out->document.find("name=\"'world'\"") == std::string::npos);
}

TEST_CASE("a bare equals separates a name from a multi-word quoted default",
          "[xacro][structural][macro]")
{
    meios::log_sink silent;
    const expansion_result out = expand_source(bare_separator_multiword, silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("xyz=\"0 0 0\"") != std::string::npos);
}

TEST_CASE("a bare equals separates a name from a single unquoted default token",
          "[xacro][structural][macro]")
{
    meios::log_sink silent;
    const expansion_result out = expand_source(bare_separator_token, silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("name=\"l0\"") != std::string::npos);
}

TEST_CASE("an inherit-or-fallback default binds the unquoted fallback text",
          "[xacro][structural][macro]")
{
    meios::log_sink silent;
    const expansion_result out = expand_source(fallback_quoted_multiword, silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("xyz=\"0 0 0\"") != std::string::npos);
}

TEST_CASE("an unquoted macro default still binds its token verbatim", "[xacro][structural][macro]")
{
    meios::log_sink silent;
    const expansion_result out = expand_source(unquoted_baseline, silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("name=\"world\"") != std::string::npos);
}

TEST_CASE("both block-parameter spellings still bind their call children",
          "[xacro][structural][macro]")
{
    meios::log_sink silent;
    const expansion_result out = expand_source(block_spellings, silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("xyz=\"1 2 3\"") != std::string::npos);
    REQUIRE(out->document.find("<mass value=\"1\"") != std::string::npos);
    REQUIRE(out->document.find("<group") == std::string::npos);
}

TEST_CASE("an unterminated quote leaves its opening quote in the bound value and refuses nothing",
          "[xacro][structural][macro]")
{
    meios::log_sink silent;
    const expansion_result out = expand_source(unterminated_quote, silent);
    REQUIRE(out.has_value());
    REQUIRE(out->document.find("name=\"'world\"") != std::string::npos);
}
