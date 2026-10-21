#include "xacro_include_tree.h"

#include "meios/io/path_grammar.h"

#include <meios/bundle/result.h>
#include <meios/bundle/manifest.h>
#include <meios/bundle/package_writer.h>

#include <meios/io/bundle_source.h>
#include <meios/io/source_lookup.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/source_location.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>

namespace
{

using meios::detail::join_under;
using meios::detail::posix_paths;
using meios::detail::windows_paths;

struct heard_record
{
    meios::diagnostic_code code;
    std::string message;
};

struct recorder
{
    std::vector<heard_record> &heard;

    void operator()(meios::level, const std::string &message)
    {
        heard.push_back({meios::diagnostic_code::unspecified, message});
    }

    void operator()(meios::level, const meios::source_location &, const std::string &message)
    {
        heard.push_back({meios::diagnostic_code::unspecified, message});
    }

    void operator()(meios::level, meios::diagnostic_code code, const meios::source_location &, const std::string &message)
    {
        heard.push_back({code, message});
    }
};

void every_record_is_an_escape(const std::vector<heard_record> &heard)
{
    for(const heard_record &one : heard)
    {
        CAPTURE(one.message);
        CHECK(one.code == meios::diagnostic_code::uncontained_asset);
        CHECK(one.message.find("escapes the configured source root") != std::string::npos);
    }
}

template<typename Source>
void refuses_rooted(Source &source, const std::string &rooted, const std::vector<heard_record> &heard)
{
    REQUIRE(source.locate("p", "m.stl").has_value());
    REQUIRE(heard.empty());
    CHECK_FALSE(source.locate("p", rooted).has_value());
    std::size_t refusals = 2;
    if constexpr(requires { source.try_locate("p", rooted); })
    {
        const meios::source_lookup_result typed = source.try_locate("p", rooted);
        REQUIRE(typed.has_value());
        CHECK_FALSE(typed->has_value());
        ++refusals;
    }
    CHECK_FALSE(source.answers("p", rooted));
    CHECK_FALSE(source.path_of("p", rooted).has_value());
    CHECK(heard.size() == refusals);
    every_record_is_an_escape(heard);
}

}

TEST_CASE("a rooted relative part names no candidate under the Windows grammar, a drive or a backslash included", "[io][join]")
{
    for(const std::string_view relative : {"D:/a/R/q/m.stl", "D:x", "\\x", "/x"})
    {
        CAPTURE(relative);
        CHECK_FALSE(join_under("R", "p", relative, windows_paths).has_value());
    }
}

TEST_CASE("under the POSIX grammar only a leading separator roots a relative part", "[io][join]")
{
    CHECK_FALSE(join_under("R", "p", "/x", posix_paths).has_value());
    CHECK(join_under("R", "p", "D:/a", posix_paths).has_value());
}

TEST_CASE("an ordinary package path joins under the root as it did", "[io][join]")
{
    const std::filesystem::path root("R");
    CHECK(join_under(root, "p", "urdf/c.xacro", windows_paths) == std::optional{root / "p" / "urdf/c.xacro"});
    CHECK(join_under(root, "p", "urdf/c.xacro", posix_paths) == std::optional{root / "p" / "urdf/c.xacro"});
    CHECK(join_under(root, "p", "", posix_paths) == std::optional{root / "p"});
    CHECK(join_under(root, "", "x", posix_paths) == std::optional{root / "" / "x"});
}

TEST_CASE("a directory or bundle source refuses a rooted relative part as an escape, even one naming a file inside its root", "[io][join]")
{
    const include_tree::tree held;
    const std::filesystem::path root = held.root() / "R";
    const std::string rooted         = held.write("R/p/m.stl", "solid\n").generic_string();
    SECTION("directory")
    {
        std::vector<heard_record> heard;
        meios::log_sink_f log{recorder{heard}};
        meios::directory_source source{root, log};
        refuses_rooted(source, rooted, heard);
    }
    SECTION("bundle")
    {
        std::vector<heard_record> heard;
        meios::log_sink_f log{recorder{heard}};
        meios::bundle_source source{root, log};
        refuses_rooted(source, rooted, heard);
    }
}

TEST_CASE("a folder bundle refuses a rooted destination name, even one inside its root, and writes nothing there", "[io][join][bundle]")
{
    const include_tree::tree held;
    const std::filesystem::path root = held.root() / "bundle";
    std::filesystem::create_directories(root);
    std::vector<heard_record> heard;
    meios::log_sink_f log{recorder{heard}};
    meios::package_writer writer{root, log};

    const meios::emit_result refused = writer.write((root / "x.urdf").generic_string(), "<robot/>", meios::asset_manifest{}, false);
    CHECK(refused.status == meios::emit_status::io_error);
    CHECK_FALSE(heard.empty());
    every_record_is_an_escape(heard);
    CHECK_FALSE(std::filesystem::exists(root / "x.urdf"));

    CHECK(writer.write("x.urdf", "<robot/>", meios::asset_manifest{}, false).status == meios::emit_status::ok);
    CHECK(std::filesystem::exists(root / "x.urdf"));
}
