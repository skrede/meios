#include "cmake_listfile_scan.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>
#include <string>
#include <filesystem>
#include <string_view>

namespace
{

using cmake_listfile::is_foreign_tree;
using cmake_listfile::listfile_scan;
using cmake_listfile::occurrence;
using cmake_listfile::scan_repository;

enum class reach
{
    file,
    subtree
};

struct exemption
{
    reach extent;
    std::string_view path;
    std::string_view variable;
    std::string_view reason;
};

constexpr std::array<exemption, 5> allowlist{
    { { reach::file, "tests/compile/pass/CMakeLists.txt", "CMAKE_BINARY_DIR",
        "a generated build system exists only at the top of the build tree; the target and "
        "configuration arguments are what narrow the build to one probe" },
      { reach::file, "tests/compile/fail/CMakeLists.txt", "CMAKE_BINARY_DIR",
        "a generated build system exists only at the top of the build tree; the target and "
        "configuration arguments are what narrow the build to one probe" },
      { reach::file, "cmake/MeiosResourceCache.cmake", "CMAKE_BINARY_DIR",
        "one resource cache and one owner key per build tree is the unit the claim and prune "
        "lifecycle rests on" },
      { reach::file, "cmake/corpus.cmake", "CMAKE_BINARY_DIR",
        "corpus scratch belongs to whichever build tree is running, not to meios's own binary "
        "directory" },
      { reach::subtree, "tests/integration/cmake/fixtures", "CMAKE_BINARY_DIR",
        "a fixture under this tree calls project() itself, so the top of the build genuinely is "
        "its own and the variable means there exactly what it says; excused by subtree because a "
        "per-line list would churn on every fixture edit without excusing anything a reader "
        "disputes" } }
};

bool under(std::string_view directory, const std::string &path)
{
    return path.size() > directory.size() && path.rfind(directory, 0) == 0
        && path[directory.size()] == '/';
}

bool covers(const exemption &entry, const occurrence &at)
{
    if(entry.variable != at.variable)
        return false;
    return entry.extent == reach::subtree ? under(entry.path, at.path) : entry.path == at.path;
}

bool exempt(const occurrence &at)
{
    for(const exemption &entry : allowlist)
        if(covers(entry, at))
            return true;
    return false;
}

bool still_occurs(const exemption &entry, const std::vector<occurrence> &occurrences)
{
    for(const occurrence &at : occurrences)
        if(covers(entry, at))
            return true;
    return false;
}

}

TEST_CASE("cmake_listfile_scope: the walk reads the repository's listfiles")
{
    const listfile_scan found = scan_repository();
    REQUIRE(found.files > 0);
    REQUIRE_FALSE(found.occurrences.empty());
}

TEST_CASE("cmake_listfile_scope: no listfile outside the allowlist names the top of the build")
{
    const listfile_scan found = scan_repository();
    REQUIRE(found.files > 0);
    for(const occurrence &at : found.occurrences)
    {
        INFO(at.path << ':' << at.line << " names " << at.variable);
        CHECK(exempt(at));
    }
}

// The walk had no such rule until a dependency's source, checked out beside this repository's own
// by CI rather than by any listfile here, put its CMAKE_SOURCE_DIR lines in front of the claim
// above. Pinned on the predicate because scan_repository reads one fixed root.
TEST_CASE("cmake_listfile_scope: a second repository's working copy is not this one's")
{
    const std::filesystem::path root
        = std::filesystem::temp_directory_path() / "meios-listfile-scope-nested";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "checked-out" / ".git");
    std::filesystem::create_directories(root / "authored");
    CHECK(is_foreign_tree(root / "checked-out"));
    CHECK_FALSE(is_foreign_tree(root / "authored"));
    std::filesystem::remove_all(root);
}

TEST_CASE("cmake_listfile_scope: every allowlist entry still excuses a live occurrence")
{
    const listfile_scan found = scan_repository();
    REQUIRE_FALSE(found.occurrences.empty());
    for(const exemption &entry : allowlist)
    {
        INFO(entry.path << ' ' << entry.variable);
        CHECK_FALSE(entry.reason.empty());
        CHECK(still_occurs(entry, found.occurrences));
    }
}
