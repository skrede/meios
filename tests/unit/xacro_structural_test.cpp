#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>

#ifndef MEIOS_GOLDEN_DIR
    #error "MEIOS_GOLDEN_DIR must name the golden-corpus root"
#endif

namespace
{

std::string read_file(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string expand_case(const std::filesystem::path &dir, meios::log_sink &log)
{
    meios::directory_source fixture{ dir, log };
    meios::source_stack sources{ std::move(fixture) };
    meios::eval_scope scope;
    std::string source = read_file(dir / "input.xacro");
    meios::expansion out = meios::expand(source, scope, sources, dir / "input.xacro",
                                         meios::expansion_limits{}, log);
    REQUIRE(out.ok);
    return meios::canonical_xml(out.document);
}

meios::expansion expand_source(const std::string &source, meios::log_sink &log)
{
    meios::source_stack sources{};
    meios::eval_scope scope;
    return meios::expand(source, scope, sources, "inline.xacro", meios::expansion_limits{}, log);
}

const char *inherit_hit =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:property name=\"a\" value=\"7\"/>"
    "<xacro:macro name=\"m\" params=\"a:=^\"><link name=\"l${a}\"/></xacro:macro>"
    "<xacro:m/></robot>";

const char *inherit_fallback =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:macro name=\"m\" params=\"a:=^|5\"><link name=\"l${a}\"/></xacro:macro>"
    "<xacro:m/></robot>";

const char *inherit_missing =
    "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
    "<xacro:macro name=\"m\" params=\"a:=^\"><link name=\"l${a}\"/></xacro:macro>"
    "<xacro:m/></robot>";

}

TEST_CASE("every structural golden expands to its expected URDF", "[xacro][structural][golden]")
{
    const std::filesystem::path root = std::filesystem::path(MEIOS_GOLDEN_DIR) / "structural";
    meios::log_sink silent;
    int cases = 0;
    for(const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(root))
    {
        if(!entry.is_directory() || !std::filesystem::exists(entry.path() / "input.xacro"))
            continue;
        INFO("case: " << entry.path().filename().string());
        std::string expected = meios::canonical_xml(read_file(entry.path() / "expected.urdf"));
        REQUIRE(expand_case(entry.path(), silent) == expected);
        ++cases;
    }
    REQUIRE(cases == 3);
}

TEST_CASE("a caret macro default inherits the enclosing binding", "[xacro][structural][macro]")
{
    meios::log_sink silent;
    const meios::expansion out = expand_source(inherit_hit, silent);
    REQUIRE(out.ok);
    REQUIRE(out.document.find("l7") != std::string::npos);
}

TEST_CASE("a caret-fallback macro default resolves the fallback when nothing is inherited",
          "[xacro][structural][macro]")
{
    meios::log_sink silent;
    const meios::expansion out = expand_source(inherit_fallback, silent);
    REQUIRE(out.ok);
    REQUIRE(out.document.find("l5") != std::string::npos);
}

TEST_CASE("a caret macro default with no inherited value and no fallback fails loudly",
          "[xacro][structural][macro]")
{
    meios::log_sink silent;
    const meios::expansion out = expand_source(inherit_missing, silent);
    REQUIRE_FALSE(out.ok);
    REQUIRE(out.document.find("^") == std::string::npos);
}
