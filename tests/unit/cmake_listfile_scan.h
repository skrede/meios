#ifndef HPP_GUARD_MEIOS_UNIT_CMAKE_LISTFILE_SCAN_H
#define HPP_GUARD_MEIOS_UNIT_CMAKE_LISTFILE_SCAN_H

#include <array>
#include <cctype>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <string_view>

namespace cmake_listfile
{

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

// A configured build tree in the working copy holds hundreds of listfiles that are not repository
// sources, and the cache file identifies one wherever a developer put it. A directory carrying its
// own .git is a second repository's working copy, whose listfiles answer to that project's
// conventions rather than to this one's; CI checks a dependency's source out beside this one's, so
// the case is routine rather than hypothetical.
inline bool is_foreign_tree(const std::filesystem::path &directory)
{
    const std::string name = directory.filename().string();
    return name == ".git" || name == "_deps" || name == "CMakeFiles"
        || std::filesystem::exists(directory / ".git")
        || std::filesystem::exists(directory / "CMakeCache.txt");
}

inline bool is_listfile(const std::filesystem::path &file)
{
    return file.filename() == "CMakeLists.txt" || file.extension() == ".cmake";
}

inline bool is_comment(const std::string &line)
{
    for(char c : line)
        if(std::isspace(static_cast<unsigned char>(c)) == 0)
            return c == '#';
    return false;
}

inline bool word_boundary(const std::string &line, std::string::size_type at, std::string_view token)
{
    const auto in_word = [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
    };
    if(at > 0 && in_word(line[at - 1]))
        return false;
    const std::string::size_type after = at + token.size();
    return after >= line.size() || !in_word(line[after]);
}

inline bool names_token(const std::string &line, std::string_view token)
{
    for(std::string::size_type at = line.find(token); at != std::string::npos;
        at = line.find(token, at + 1))
        if(word_boundary(line, at, token))
            return true;
    return false;
}

inline void collect(const std::string &path, int number, const std::string &line,
                    std::vector<occurrence> &out)
{
    if(is_comment(line))
        return;
    for(std::string_view token : top_of_build)
        if(names_token(line, token))
            out.push_back({ path, number, token });
}

inline std::string repository_relative(const std::filesystem::path &file)
{
    const std::filesystem::path root{ MEIOS_REPOSITORY_ROOT };
    return std::filesystem::relative(file, root).generic_string();
}

inline void read_listfile(const std::filesystem::path &file, listfile_scan &found)
{
    const std::string path = repository_relative(file);
    std::ifstream stream{ file };
    std::string line;
    int number = 0;
    while(std::getline(stream, line))
        collect(path, ++number, line, found.occurrences);
    ++found.files;
}

inline void visit(std::filesystem::recursive_directory_iterator &walk, listfile_scan &found)
{
    if(walk->is_directory())
    {
        if(is_foreign_tree(walk->path()))
            walk.disable_recursion_pending();
        return;
    }
    if(is_listfile(walk->path()))
        read_listfile(walk->path(), found);
}

inline listfile_scan scan_repository()
{
    const std::filesystem::path root{ MEIOS_REPOSITORY_ROOT };
    listfile_scan found{ 0, {} };
    const std::filesystem::recursive_directory_iterator end;
    for(std::filesystem::recursive_directory_iterator walk{ root }; walk != end; ++walk)
        visit(walk, found);
    return found;
}

}

#endif
