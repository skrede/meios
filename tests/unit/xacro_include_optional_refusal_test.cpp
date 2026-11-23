#include "xacro_include_optional_fixture.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/memory_source.h>

#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <filesystem>

namespace
{

meios::memory_source served_optional_climb(meios::log_sink &log)
{
    meios::memory_source held{log};
    held.add("pkg", "urdf/a.xacro", include_tree::robot("<xacro:include filename=\"../../other/x.xacro\" optional=\"true\"/>"));
    held.add("other", "x.xacro", include_tree::binds("other"));
    return held;
}

include_tree::expansion_result expand_climbing(meios::source_stack &sources, meios::log_sink &log)
{
    meios::include_options includes;
    includes.climb_above_document = true;
    meios::eval_scope scope;
    const std::string text = include_tree::robot(include_tree::include("package://pkg/urdf/a.xacro") + "<link name=\"${leaf}\"/>");
    return meios::expand(text, scope, sources, std::filesystem::path{}, meios::expansion_limits{}, meios::eval_policy::fail, {}, log, includes);
}

}

TEST_CASE("an optional include of a link leaving every admitting directory refuses as uncontained", "[xacro][include][optional]")
{
    const include_tree::tree held;
    const std::filesystem::path target = held.write("elsewhere/a.xacro", include_tree::binds("leaf"));
    std::filesystem::create_directories(held.root() / "doc");
    if(!include_tree::linked(target, held.root() / "doc" / "a.xacro"))
        SKIP("this host cannot create a symlink");
    std::vector<heard> records;
    refuses(expand_top(held, optional_include("a.xacro"), records), records, meios::diagnostic_code::uncontained_asset);
}

TEST_CASE("an optional package include of a link out of its source root refuses", "[xacro][include][optional]")
{
    const include_tree::tree held;
    const std::filesystem::path target = held.write("elsewhere/a.xacro", include_tree::binds("leaf"));
    std::filesystem::create_directories(held.root() / "root" / "p");
    if(!include_tree::linked(target, held.root() / "root" / "p" / "a.xacro"))
        SKIP("this host cannot create a symlink");
    std::vector<heard> records;
    refuses(expand_top(held, optional_include("package://p/a.xacro"), records), records, meios::diagnostic_code::unresolved_include);
}

TEST_CASE("an optional include climbing above its document's directory refuses", "[xacro][include][optional]")
{
    const include_tree::tree held;
    held.write("a.xacro", include_tree::binds("leaf"));
    std::vector<heard> records;
    refuses(expand_top(held, optional_include("../a.xacro"), records), records, meios::diagnostic_code::xacro_structural_error);
}

TEST_CASE("an optional include of a package no source holds refuses", "[xacro][include][optional]")
{
    const include_tree::tree held;
    std::filesystem::create_directories(held.root() / "root" / "p");
    std::vector<heard> find_records;
    refuses(expand_top(held, optional_include("$(find nopkg)/a.xacro"), find_records), find_records, meios::diagnostic_code::unresolved_find);
    std::vector<heard> package_records;
    refuses(expand_top(held, optional_include("package://nopkg/a.xacro"), package_records), package_records, meios::diagnostic_code::unresolved_include);
}

TEST_CASE("an optional include of malformed XML refuses as a parse error", "[xacro][include][optional]")
{
    const include_tree::tree held;
    held.write("doc/bad.xacro", "<robot><link></robot>");
    std::vector<heard> records;
    refuses(expand_top(held, optional_include("bad.xacro"), records), records, meios::diagnostic_code::xacro_parse_error);
}

TEST_CASE("an include missing inside a file an optional include read refuses", "[xacro][include][optional]")
{
    const include_tree::tree held;
    held.write("doc/outer.xacro", include_tree::robot(include_tree::include("missing.xacro")));
    std::vector<heard> records;
    refuses(expand_top(held, optional_include("outer.xacro"), records), records, meios::diagnostic_code::unresolved_include);
}

TEST_CASE("an optional include re-entering the file that writes it refuses as a cycle", "[xacro][include][optional]")
{
    const include_tree::tree held;
    std::vector<heard> records;
    const include_tree::expansion_result out = expand_top(held, optional_include("top.xacro"), records);
    refuses(out, records, meios::diagnostic_code::xacro_structural_error);
    REQUIRE(out.error().message.find("cycle") != std::string::npos);
}

TEST_CASE("an optional include a served document writes, climbing out of its package, refuses with the climb admitted", "[xacro][include][optional]")
{
    std::vector<heard> records;
    meios::log_sink_f log{listener{records}};
    meios::source_stack sources{served_optional_climb(log)};
    const include_tree::expansion_result out = expand_climbing(sources, log);
    refuses(out, records, meios::diagnostic_code::xacro_structural_error);
    REQUIRE(out.error().message.find("\"../../other/x.xacro\"") != std::string::npos);
    REQUIRE(out.error().message.find("\"pkg\"") != std::string::npos);
}
