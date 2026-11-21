#include "xacro_read_probe.h"
#include "xacro_include_tree.h"

#include <meios/io/text_reader.h>
#include <meios/io/source_stack.h>
#include <meios/io/package_source.h>
#include <meios/io/resolved_asset.h>

#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <string_view>
#include <system_error>

#if defined(__linux__)
    #include <unistd.h>
#endif

namespace
{

bool make_link(const std::filesystem::path &target, const std::filesystem::path &link, bool directory)
{
    std::error_code error;
    if(directory)
        std::filesystem::create_directory_symlink(target, link, error);
    else
        std::filesystem::create_symlink(target, link, error);
    return !error;
}

// Answers the one include with the asset it was built with, as a layer whose path was swapped for a
// link between its locate and the open would, and holds the package whatever it answers.
struct pinned_layer
{
    std::optional<meios::resolved_asset> answer;

    static meios::capability_descriptor capabilities()
    {
        return {meios::source_kind::directory, false};
    }

    std::vector<std::string> packages() const
    {
        return {"p"};
    }

    std::optional<meios::resolved_asset> locate(std::string_view package, std::string_view relative)
    {
        if(package == "p" && relative == "inc.xacro")
            return answer;
        return std::nullopt;
    }
};

meios::text_read_failure_kind failed_kind(const std::filesystem::path &root, const std::filesystem::path &relative)
{
    const meios::text_read_result result = meios::read_text_file_under(root, relative);
    REQUIRE_FALSE(result.has_value());
    return result.error().kind;
}

include_tree::expansion_result expand_with(const include_tree::tree &held, std::optional<meios::resolved_asset> answer, std::vector<xacro_probe::captured> &records)
{
    const std::string body               = "\n<xacro:include filename=\"package://p/inc.xacro\" optional=\"true\"/>\n<link name=\"l\"/>";
    const std::filesystem::path document = held.write("doc/top.xacro", include_tree::robot(body));
    meios::log_sink_f log{xacro_probe::recorder{records}};
    meios::source_stack sources{pinned_layer{std::move(answer)}};
    return include_tree::expand_file(document, sources, log);
}

std::size_t notes(const std::vector<xacro_probe::captured> &records)
{
    return static_cast<std::size_t>(std::ranges::count(records, meios::level::info, &xacro_probe::captured::lvl));
}

void refuses_unresolved(const include_tree::expansion_result &out, const std::vector<xacro_probe::captured> &records)
{
    REQUIRE_FALSE(out.has_value());
    REQUIRE(out.error().code == meios::diagnostic_code::unresolved_include);
    REQUIRE(notes(records) == 0);
}

}

TEST_CASE("a rooted read refused at a link or a step out is uncontained, and a missing file is not", "[xacro][include][uncontained]")
{
    const include_tree::tree held;
    const include_tree::tree outside;
    const std::filesystem::path tree = held.root();
    held.write("inside/ok.txt", "inside");
    outside.write("outside.txt", "outside");
    if(!make_link(tree / "inside" / "ok.txt", tree / "file-link", false) || !make_link(tree / "inside", tree / "directory-link", true) ||
       !make_link(outside.root() / "outside.txt", tree / "outside-link", false) || !make_link(tree, outside.root() / "root-link", true))
        SKIP("this host cannot create a symlink");

    REQUIRE(meios::read_text_file_under(tree, "inside/ok.txt").has_value());
    for(const std::filesystem::path relative : {"file-link", "directory-link/ok.txt", "outside-link", "../outside.txt"})
    {
        CAPTURE(relative.string());
        REQUIRE(failed_kind(tree, relative) == meios::text_read_failure_kind::uncontained);
    }
    REQUIRE(failed_kind(outside.root() / "root-link", "inside/ok.txt") == meios::text_read_failure_kind::uncontained);
    REQUIRE(failed_kind(tree, "inside/absent.txt") == meios::text_read_failure_kind::open);
    REQUIRE(failed_kind(tree, "inside") == meios::text_read_failure_kind::non_regular);
}

TEST_CASE("an optional include whose served path passes through a directory link refuses with no note", "[xacro][include][uncontained]")
{
    const include_tree::tree held;
    const std::filesystem::path package = held.root() / "root" / "p";
    held.write("root/p/real/inc.xacro", include_tree::binds("leaf"));
    if(!make_link(package / "real", package / "swapped", true))
        SKIP("this host cannot create a symlink");
    std::vector<xacro_probe::captured> records;
    const meios::resolved_asset answer{package / "swapped" / "inc.xacro", package, "swapped/inc.xacro", package / "inc.xacro"};
    refuses_unresolved(expand_with(held, answer, records), records);
}

TEST_CASE("an optional include the layer holds no file for is skipped with its note", "[xacro][include][uncontained]")
{
    const include_tree::tree held;
    std::vector<xacro_probe::captured> records;
    const include_tree::expansion_result out = expand_with(held, std::nullopt, records);
    REQUIRE(include_tree::renders_link(out, "l"));
    REQUIRE(notes(records) == 1);
}

#if defined(__linux__)
TEST_CASE("an optional include opened and then failing to read refuses with no note", "[xacro][include][uncontained]")
{
    const std::filesystem::path process  = std::filesystem::path("/proc") / std::to_string(::getpid());
    const meios::text_read_result direct = meios::read_text_file_under(process, "mem");
    REQUIRE_FALSE(direct.has_value());
    REQUIRE(direct.error().kind == meios::text_read_failure_kind::read);

    const include_tree::tree held;
    std::vector<xacro_probe::captured> records;
    refuses_unresolved(expand_with(held, meios::resolved_asset{process / "mem", process, "mem", process / "mem"}, records), records);
}
#endif
