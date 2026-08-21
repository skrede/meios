#ifndef HPP_GUARD_MEIOS_UNIT_COVERAGE_SOURCES_H
#define HPP_GUARD_MEIOS_UNIT_COVERAGE_SOURCES_H

#include <string>
#include <vector>
#include <cctype>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <optional>
#include <filesystem>
#include <string_view>

#ifndef MEIOS_REPOSITORY_ROOT
    #error "MEIOS_REPOSITORY_ROOT must name the checkout the cited references live in"
#endif

namespace coverage
{

struct reference
{
    std::string path;
    std::size_t line;
};

inline std::filesystem::path in_repository(std::string_view relative)
{
    return std::filesystem::path{ MEIOS_REPOSITORY_ROOT } / relative;
}

inline std::vector<std::string> words_of(std::string_view text)
{
    std::istringstream stream{ std::string(text) };
    std::vector<std::string> out;
    for(std::string word; stream >> word;)
        out.push_back(word);
    return out;
}

inline bool is_reference(const std::string &word)
{
    return word.starts_with("tests/") || word.starts_with(".github/");
}

// A reference names a case by where it sits, so the trailing line number is part of what gets
// resolved: a file too short to hold the cited line is a reference to something that moved. A
// path ending a sentence keeps the sentence's punctuation, and no path ends in any of it.
inline reference cited_by(const std::string &word)
{
    const std::string::size_type last = word.find_last_not_of(".,;:");
    const std::string trimmed = word.substr(0, last + 1);
    const std::string::size_type colon = trimmed.rfind(':');
    if(colon == std::string::npos
       || trimmed.find_first_not_of("0123456789", colon + 1) != std::string::npos)
        return { trimmed, 0 };
    return { trimmed.substr(0, colon), std::stoul(trimmed.substr(colon + 1)) };
}

inline std::size_t lines_in(const std::filesystem::path &path)
{
    std::ifstream in(path);
    std::size_t counted = 0;
    for(std::string line; std::getline(in, line);)
        ++counted;
    return counted;
}

// A stem missing from a foreach list is a file that silently never runs, so a cited unit or
// property source is resolved against the listfiles as well as against the filesystem. The fuzz
// and compile directories are registered by glob and the golden ones are read as data, so for
// those the file's existence is the whole of what registration can mean.
inline bool needs_registration(const std::string &path)
{
    return path.ends_with(".cpp")
           && (path.starts_with("tests/unit/") || path.starts_with("tests/property/"));
}

inline std::string stem_of(const std::filesystem::path &path)
{
    std::string stem = path.stem().string();
    if(stem.ends_with("_test"))
        stem.resize(stem.size() - 5);
    return stem;
}

inline bool names_word(const std::string &line, const std::string &token)
{
    std::string word;
    for(char c : line + " ")
    {
        if(std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_')
        {
            word += c;
            continue;
        }
        if(word == token)
            return true;
        word.clear();
    }
    return false;
}

inline std::vector<std::filesystem::path> listfiles()
{
    std::vector<std::filesystem::path> found{ in_repository("tests/CMakeLists.txt") };
    for(const std::filesystem::directory_entry &entry :
        std::filesystem::directory_iterator{ in_repository("tests/cmake") })
        if(entry.path().extension() == ".cmake")
            found.push_back(entry.path());
    return found;
}

inline bool registered(const std::string &stem)
{
    for(const std::filesystem::path &file : listfiles())
    {
        std::ifstream in(file);
        for(std::string line; std::getline(in, line);)
            if(names_word(line, stem))
                return true;
    }
    return false;
}

inline std::optional<std::string> member_on(const std::string &line)
{
    constexpr std::string_view opening = "    std::size_t ";
    if(!line.starts_with(opening) || !line.ends_with(";"))
        return std::nullopt;
    const std::string named = line.substr(opening.size(), line.size() - opening.size() - 1);
    if(named.find_first_of("(=<> ") != std::string::npos)
        return std::nullopt;
    return named;
}

// The axis list is paired against the members an aggregate declares rather than against a
// transcribed copy of them: a transcribed list drifts, and drift here is exactly how a file ends
// up total against eleven axes while the twelfth takes the process down.
inline std::vector<std::string> declared_axes(std::string_view header, std::string_view aggregate)
{
    std::ifstream in(in_repository(header));
    const std::string opening = "struct " + std::string(aggregate);
    std::vector<std::string> members;
    bool inside = false;
    for(std::string line; std::getline(in, line);)
    {
        if(!inside)
        {
            inside = line.find(opening) != std::string::npos;
            continue;
        }
        if(line.starts_with("};"))
            break;
        if(const std::optional<std::string> named = member_on(line))
            members.push_back(*named);
    }
    return members;
}

inline std::vector<std::string> evaluator_axes()
{
    return declared_axes("lib/meios-xacro/include/meios/xacro/evaluator_limits.h",
                         "evaluator_limits");
}

inline std::vector<std::string> expansion_axes()
{
    return declared_axes("lib/meios-xacro/include/meios/xacro/budget.h", "expansion_limits");
}

}

#endif
