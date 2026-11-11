#include "xacro_include_tree.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/source_location.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <utility>
#include <algorithm>
#include <filesystem>

namespace
{

struct heard
{
    meios::level lvl;
    int32_t line;
    std::string message;
};

struct listener
{
    std::vector<heard> &sink;

    void operator()(meios::level lvl, const std::string &message) { sink.push_back({ lvl, 0, message }); }
    void operator()(meios::level lvl, const meios::source_location &where, const std::string &message) { sink.push_back({ lvl, where.line, message }); }
};

include_tree::expansion_result expand_top(const include_tree::tree &held, const std::string &body, std::vector<heard> &records)
{
    const std::filesystem::path document = held.write("doc/top.xacro", include_tree::robot(body));
    meios::log_sink_f log{ listener{ records } };
    meios::source_stack sources{ meios::directory_source{ held.root() / "root", log } };
    return include_tree::expand_file(document, sources, log);
}

std::string optional_include(const std::string &filename, const std::string &optional = "true")
{
    return "\n<xacro:include filename=\"" + filename + "\" optional=\"" + optional + "\"/>\n<link name=\"l\"/>";
}

std::size_t notes(const std::vector<heard> &records)
{
    return static_cast<std::size_t>(std::ranges::count(records, meios::level::info, &heard::lvl));
}

std::string skipped(const include_tree::expansion_result &out, const std::vector<heard> &records, const std::string &filename)
{
    REQUIRE(include_tree::renders_link(out, "l"));
    REQUIRE(notes(records) == 1);
    const heard &note = *std::ranges::find(records, meios::level::info, &heard::lvl);
    REQUIRE(note.line == 2);
    REQUIRE(note.message.starts_with("optional include \"" + filename + "\" was not read: "));
    REQUIRE(note.message.ends_with("; nothing was included"));
    return note.message;
}

void refuses(const include_tree::expansion_result &out, const std::vector<heard> &records, meios::diagnostic_code code)
{
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == code);
    REQUIRE(notes(records) == 0);
}

}

TEST_CASE("an optional include of a missing file includes nothing and notes it at the include", "[xacro][include][optional]")
{
    const include_tree::tree held;
    std::vector<heard> records;
    const include_tree::expansion_result out = expand_top(held, optional_include("missing.xacro"), records);
    skipped(out, records, "missing.xacro");
    REQUIRE(meios::canonical_xml(out->document) == R"(<robot name="probe"><link name="l"></link></robot>)");
}

TEST_CASE("an include of a missing file without optional still refuses", "[xacro][include][optional]")
{
    const include_tree::tree held;
    std::vector<heard> records;
    refuses(expand_top(held, "\n<xacro:include filename=\"missing.xacro\"/>\n<link name=\"l\"/>", records), records, meios::diagnostic_code::unresolved_include);
}

TEST_CASE("an optional include of a directory includes nothing and notes it", "[xacro][include][optional]")
{
    const include_tree::tree held;
    std::filesystem::create_directories(held.root() / "doc" / "found.xacro");
    std::vector<heard> records;
    REQUIRE(skipped(expand_top(held, optional_include("found.xacro"), records), records, "found.xacro").find("not a regular file") != std::string::npos);
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

TEST_CASE("an optional include of a file missing from a held package is skipped", "[xacro][include][optional]")
{
    const include_tree::tree held;
    std::filesystem::create_directories(held.root() / "root" / "p");
    std::vector<heard> package_records;
    skipped(expand_top(held, optional_include("package://p/missing.xacro"), package_records), package_records, "package://p/missing.xacro");
    std::vector<heard> find_records;
    skipped(expand_top(held, optional_include("$(find p)/missing.xacro"), find_records), find_records, "$(find p)/missing.xacro");
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

TEST_CASE("optional is read by the boolean rule once evaluated", "[xacro][include][optional]")
{
    const include_tree::tree held;
    for(const char *truthy : { "1", "${True}" })
    {
        std::vector<heard> records;
        skipped(expand_top(held, optional_include("missing.xacro", truthy), records), records, "missing.xacro");
    }
    const std::vector<std::pair<std::string, meios::diagnostic_code>> refused{ { "false", meios::diagnostic_code::unresolved_include },
                                                                               { "0", meios::diagnostic_code::unresolved_include },
                                                                               { "", meios::diagnostic_code::xacro_structural_error },
                                                                               { "yes", meios::diagnostic_code::xacro_structural_error } };
    for(const auto &[text, code] : refused)
    {
        std::vector<heard> records;
        refuses(expand_top(held, optional_include("missing.xacro", text), records), records, code);
    }
}

TEST_CASE("an optional include of a file this process may not read is skipped", "[xacro][include][optional]")
{
    const include_tree::tree held;
    const std::filesystem::path denied = held.write("doc/denied.xacro", include_tree::binds("leaf"));
    std::filesystem::permissions(denied, std::filesystem::perms::none);
    const bool readable = std::ifstream(denied).is_open();
    std::vector<heard> records;
    const include_tree::expansion_result out = expand_top(held, optional_include("denied.xacro"), records);
    std::filesystem::permissions(denied, std::filesystem::perms::owner_all);
    if(readable)
        SKIP("this host reads a file whatever its permissions say, as an administrator or where permissions do not deny reads");
    REQUIRE(skipped(out, records, "denied.xacro").find("open failed") != std::string::npos);
}
