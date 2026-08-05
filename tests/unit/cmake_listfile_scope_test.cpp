#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cctype>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <string_view>

namespace
{

struct exemption
{
    std::string_view path;
    std::string_view variable;
    std::string_view reason;
};

struct occurrence
{
    std::string path;
    int line;
    std::string_view variable;
};

struct listfile_scan
{
    int files;
    std::vector<occurrence> occurrences;
};

constexpr std::array<std::string_view, 2> top_of_build{ "CMAKE_SOURCE_DIR", "CMAKE_BINARY_DIR" };

// A fixture under this tree calls project() itself, so the top of the build genuinely is its own
// and the variables mean there exactly what they say. Exempt by prefix rather than line by line:
// a per-line list would churn on every fixture edit without excusing anything a reader disputes.
constexpr std::string_view fixture_tree = "tests/integration/cmake/fixtures";

constexpr std::array<exemption, 4> allowlist{
    { { "tests/compile/pass/CMakeLists.txt", "CMAKE_BINARY_DIR",
        "a generated build system exists only at the top of the build tree; the target and "
        "configuration arguments are what narrow the build to one probe" },
      { "tests/compile/fail/CMakeLists.txt", "CMAKE_BINARY_DIR",
        "a generated build system exists only at the top of the build tree; the target and "
        "configuration arguments are what narrow the build to one probe" },
      { "cmake/MeiosResourceCache.cmake", "CMAKE_BINARY_DIR",
        "one resource cache and one owner key per build tree is the unit the claim and prune "
        "lifecycle rests on" },
      { "cmake/corpus.cmake", "CMAKE_BINARY_DIR",
        "corpus scratch belongs to whichever build tree is running, not to meios's own binary "
        "directory" } }
};

// A configured build tree in the working copy holds hundreds of listfiles that are not repository
// sources, and the cache file identifies one wherever a developer put it.
bool is_generated_tree(const std::filesystem::path &directory)
{
    const std::string name = directory.filename().string();
    return name == ".git" || name == "_deps" || name == "CMakeFiles"
        || std::filesystem::exists(directory / "CMakeCache.txt");
}

bool is_listfile(const std::filesystem::path &file)
{
    return file.filename() == "CMakeLists.txt" || file.extension() == ".cmake";
}

bool is_comment(const std::string &line)
{
    for(char c : line)
        if(std::isspace(static_cast<unsigned char>(c)) == 0)
            return c == '#';
    return false;
}

bool word_boundary(const std::string &line, std::string::size_type at, std::string_view token)
{
    const auto in_word = [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
    };
    if(at > 0 && in_word(line[at - 1]))
        return false;
    const std::string::size_type after = at + token.size();
    return after >= line.size() || !in_word(line[after]);
}

bool names_token(const std::string &line, std::string_view token)
{
    for(std::string::size_type at = line.find(token); at != std::string::npos;
        at = line.find(token, at + 1))
        if(word_boundary(line, at, token))
            return true;
    return false;
}

void collect(const std::string &path, int number, const std::string &line,
             std::vector<occurrence> &out)
{
    if(is_comment(line))
        return;
    for(std::string_view token : top_of_build)
        if(names_token(line, token))
            out.push_back({ path, number, token });
}

std::string repository_relative(const std::filesystem::path &file)
{
    const std::filesystem::path root{ MEIOS_REPOSITORY_ROOT };
    return std::filesystem::relative(file, root).generic_string();
}

void read_listfile(const std::filesystem::path &file, listfile_scan &found)
{
    const std::string path = repository_relative(file);
    std::ifstream stream{ file };
    std::string line;
    int number = 0;
    while(std::getline(stream, line))
        collect(path, ++number, line, found.occurrences);
    ++found.files;
}

void visit(std::filesystem::recursive_directory_iterator &walk, listfile_scan &found)
{
    if(walk->is_directory())
    {
        if(is_generated_tree(walk->path()))
            walk.disable_recursion_pending();
        return;
    }
    if(is_listfile(walk->path()))
        read_listfile(walk->path(), found);
}

listfile_scan scan_repository()
{
    const std::filesystem::path root{ MEIOS_REPOSITORY_ROOT };
    listfile_scan found{ 0, {} };
    const std::filesystem::recursive_directory_iterator end;
    for(std::filesystem::recursive_directory_iterator walk{ root }; walk != end; ++walk)
        visit(walk, found);
    return found;
}

bool exempt(const occurrence &at)
{
    if(at.path.rfind(fixture_tree, 0) == 0)
        return true;
    for(const exemption &entry : allowlist)
        if(entry.path == at.path && entry.variable == at.variable)
            return true;
    return false;
}

bool still_occurs(const exemption &entry, const std::vector<occurrence> &occurrences)
{
    for(const occurrence &at : occurrences)
        if(at.path == entry.path && at.variable == entry.variable)
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
