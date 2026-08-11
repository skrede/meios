#ifndef HPP_GUARD_CONSUMER_INTERPRETER_ABSENCE_H
#define HPP_GUARD_CONSUMER_INTERPRETER_ABSENCE_H

#include <string>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace consumer
{

#ifdef _WIN32
inline constexpr char path_separator = ';';
inline constexpr std::string_view interpreter_names[] = { "python.exe", "python3.exe", "py.exe" };
#else
inline constexpr char path_separator = ':';
inline constexpr std::string_view interpreter_names[] = { "python", "python3" };
#endif

inline int refuse(const std::string &what)
{
    std::cerr << what << '\n';
    return 1;
}

// A versioned spelling extends a bare one with a dot and the version, on either platform:
// python3.13 where executables carry no extension, python3.13.exe where they do.
inline bool names_interpreter(const std::string &entry)
{
    for(const std::string_view name : interpreter_names)
        if(entry == name)
            return true;
    return entry.rfind("python2.", 0) == 0 || entry.rfind("python3.", 0) == 0;
}

inline std::string interpreter_in(const std::string &dir)
{
    std::error_code unreadable;
    for(const std::filesystem::directory_entry &entry :
        std::filesystem::directory_iterator(dir, unreadable))
    {
        const std::string name = entry.path().filename().string();
        if(names_interpreter(name))
            return (std::filesystem::path(dir) / name).string();
    }
    return {};
}

inline std::string interpreter_on(const std::string &path)
{
    for(std::string rest = path; !rest.empty();)
    {
        const std::size_t split_at = rest.find(path_separator);
        const std::string dir = rest.substr(0, split_at);
        rest = split_at == std::string::npos ? std::string{} : rest.substr(split_at + 1);
        if(dir.empty())
            continue;
        const std::string found = interpreter_in(dir);
        if(!found.empty())
            return found;
    }
    return {};
}

// The claim is defensible only against a path that is set and empty, and it is decided on that
// emptiness rather than on what a scan found. An unset path is not an absence of candidates:
// POSIX execvp falls back to confstr(_CS_PATH) when PATH is not in the environment, so a system
// interpreter stays spawnable while a scan of nothing reports nothing. The scan survives because
// it is what makes a refusal actionable -- it names the directory and the executable it matched.
inline int refuse_reachable_interpreter()
{
    const char *path = std::getenv("PATH");
    if(path == nullptr)
        return refuse("the process path is unset, so the system default path still resolves an "
                      "interpreter and this run cannot state that the load needed none");
    const std::string rest = path;
    if(rest.empty())
        return 0;
    const std::string found = interpreter_on(rest);
    if(found.empty())
        return refuse("the process path is not empty, so this run cannot state that the load "
                      "needed no interpreter; it names " + rest);
    return refuse("an interpreter is reachable at " + found
                  + ", so this run cannot state that the load needed none");
}

// The arming fact and the fact under inspection come from different sources on purpose: an
// invocation that lost the environment emptying the path is left armed against a full path, so
// the leg reddens instead of quietly not existing.
inline bool claims_no_interpreter(int argc, char **argv)
{
    for(int at = 1; at < argc; ++at)
        if(std::string_view(argv[at]) == "--no-interpreter")
            return true;
    return std::getenv("MEIOS_CONSUMER_NO_INTERPRETER") != nullptr;
}

// The one entry point both probes' main() calls: arming and inspection stay as two functions
// above, but every caller wants the pair run together.
inline int guard_interpreter_absence(int argc, char **argv)
{
    if(!claims_no_interpreter(argc, argv))
        return 0;
    return refuse_reachable_interpreter();
}

}

#endif
