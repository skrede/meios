#include "meios/xacro/include_target.h"

#include "meios/io/path_grammar.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>
#include <initializer_list>

namespace
{

using meios::detail::path_grammar;
using meios::detail::posix_paths;
using meios::detail::windows_paths;
using meios::detail::directory_prefix;

struct row
{
    std::string_view written;
    std::string_view kept;
};

void check_kept(std::initializer_list<row> rows, const path_grammar &grammar)
{
    for(const row &each : rows)
    {
        CAPTURE(each.written);
        bool escaped = true;
        CHECK(meios::detail::normalize_relative(each.written, escaped, grammar) == each.kept);
        CHECK_FALSE(escaped);
    }
}

bool escapes(std::string_view written, const path_grammar &grammar)
{
    bool escaped = false;
    meios::detail::normalize_relative(written, escaped, grammar);
    return escaped;
}

}

TEST_CASE("an absolute spelling loses its root before it is walked, under either grammar", "[xacro][include][target]")
{
    check_kept({ { "D:/a/R/p./urdf/c.xacro", "a/R/p./urdf/c.xacro" },
                 { "D:\\a\\R\\p\\urdf\\c.xacro", "a/R/p/urdf/c.xacro" },
                 { "D:x/y.xacro", "x/y.xacro" },
                 { "/a/b.xacro", "a/b.xacro" } },
               windows_paths);
    check_kept({ { "/a/b.xacro", "a/b.xacro" }, { "//a/b.xacro", "a/b.xacro" } }, posix_paths);
}

TEST_CASE("a parent element right after the root escapes, under either grammar", "[xacro][include][target]")
{
    CHECK(escapes("D:/../x.xacro", windows_paths));
    CHECK(escapes("/../x.xacro", posix_paths));
    check_kept({ { "D:/a/../x.xacro", "x.xacro" } }, windows_paths);
}

TEST_CASE("a drive the walk leaves leading is removed, so the result is never rooted", "[xacro][include][target]")
{
    check_kept({ { "./D:/a/R/p/./c.xacro", "a/R/p/c.xacro" },
                 { "./D:/a/R/p/urdf/../urdf/c.xacro", "a/R/p/urdf/c.xacro" },
                 { "x/../D:/a.xacro", "a.xacro" },
                 { "x/D:/a.xacro", "x/D:/a.xacro" } },
               windows_paths);
}

TEST_CASE("a parent element a removed drive leaves leading escapes under the Windows grammar and stays a name under the POSIX one", "[xacro][include][target]")
{
    for(const std::string_view spelling : { "x/../D:..", "/D:../q", "D:D:..", "x/../D:../q/f.xacro", "C:\\x\\..\\D:..\\q" })
    {
        CAPTURE(spelling);
        CHECK(escapes(spelling, windows_paths));
    }
    check_kept({ { "x/D:../q", "x/D:../q" } }, windows_paths);
    check_kept({ { "x/../D:..", "D:.." } }, posix_paths);
}

TEST_CASE("under the POSIX grammar a drive is an ordinary name", "[xacro][include][target]")
{
    check_kept({ { "D:/a/b.xacro", "D:/a/b.xacro" }, { "./D:/a.xacro", "D:/a.xacro" } }, posix_paths);
    CHECK(directory_prefix("./D:/a/R/p/urdf/c.xacro", "D:/a/R/p", posix_paths) == std::optional<std::size_t>(10));
}

TEST_CASE("a spelling is under a find's directory only where both are rooted alike, a drive included", "[xacro][include][target]")
{
    CHECK(directory_prefix("D:/a/R/p/urdf/c.xacro", "D:/a/R/p", windows_paths) == std::optional<std::size_t>(8));
    CHECK_FALSE(directory_prefix("./D:/a/R/p/urdf/c.xacro", "D:/a/R/p", windows_paths));
    CHECK_FALSE(directory_prefix("D:/a/R/p./urdf/c.xacro", "D:/a/R/p", windows_paths));
    CHECK_FALSE(directory_prefix("C:/a/R/p/urdf/c.xacro", "D:/a/R/p", windows_paths));
    CHECK(directory_prefix("/a/R/p/urdf", "/a/R/p", posix_paths) == std::optional<std::size_t>(6));
    CHECK_FALSE(directory_prefix("/C:/a/R/p/x", "C:/a/R/p", windows_paths));
    CHECK_FALSE(directory_prefix("\\\\C:\\a\\R\\p\\x", "C:/a/R/p", windows_paths));
    CHECK_FALSE(directory_prefix("C:/a/R/p/x", "/C:/a/R/p", windows_paths));
    CHECK(directory_prefix("D://a/R/p/x", "D:/a/R/p", windows_paths) == std::optional<std::size_t>(9));
    CHECK(directory_prefix("//a/R/p/urdf", "/a/R/p", posix_paths) == std::optional<std::size_t>(7));
}

TEST_CASE("a UNC or device spelling loses only its leading separators under the Windows grammar", "[xacro][include][target]")
{
    check_kept({ { "//server/share/x.xacro", "server/share/x.xacro" },
                 { "\\\\server\\share\\x.xacro", "server/share/x.xacro" },
                 { "\\\\?\\C:\\x.xacro", "?/C:/x.xacro" } },
               windows_paths);
}

TEST_CASE("the native grammar roots a spelling exactly where the standard library finds a root path", "[xacro][include][target]")
{
    for(const std::string_view spelling : { "", "a/b", "./a", "/a", "\\a", "D:", "D:a", "D:/a", "./D:/a", "x/D:/a", "1:/a" })
    {
        CAPTURE(spelling);
        CHECK(meios::detail::rooted(spelling) == std::filesystem::path(spelling).has_root_path());
    }
}
