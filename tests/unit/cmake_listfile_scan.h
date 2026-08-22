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

inline void read_listfile(const std::string &path, listfile_scan &found)
{
    const std::filesystem::path root{ MEIOS_REPOSITORY_ROOT };
    std::ifstream stream{ root / path };
    std::string line;
    int number = 0;
    while(std::getline(stream, line))
        collect(path, ++number, line, found.occurrences);
    ++found.files;
}

// The subject is what the repository tracks, not what happens to sit in the working copy. A walk of
// the tree reads whatever a build or a job left behind -- a fetched dependency's source, an install
// prefix, a configured build tree -- and no marker distinguishes all of those from a source
// directory, so the set is taken from version control instead of guessed at from the filesystem.
inline listfile_scan scan_repository()
{
    listfile_scan found{ 0, {} };
    std::ifstream manifest{ std::filesystem::path{ MEIOS_TRACKED_LISTFILES } };
    std::string path;
    while(std::getline(manifest, path))
        if(!path.empty())
            read_listfile(path, found);
    return found;
}

}

#endif
