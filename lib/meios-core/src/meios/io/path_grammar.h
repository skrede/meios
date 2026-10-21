#ifndef HPP_GUARD_MEIOS_CORE_IO_PATH_GRAMMAR_H
#define HPP_GUARD_MEIOS_CORE_IO_PATH_GRAMMAR_H

#include <string>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

// Textual rather than std::filesystem::path, so a drive spelling is judged alike on every host and a
// Windows reading is observable where the standard library has no drive grammar.
struct path_grammar
{
    bool drive_roots;
    std::string_view separators;
};

inline constexpr path_grammar posix_paths{ .drive_roots = false, .separators = "/" };
inline constexpr path_grammar windows_paths{ .drive_roots = true, .separators = "/\\" };
inline constexpr path_grammar native_paths = std::filesystem::path::preferred_separator == '\\' ? windows_paths : posix_paths;

inline std::size_t root_length(std::string_view text, const path_grammar &grammar = native_paths)
{
    const bool letter  = !text.empty() && ((text[0] >= 'A' && text[0] <= 'Z') || (text[0] >= 'a' && text[0] <= 'z'));
    std::size_t length = grammar.drive_roots && letter && text.size() >= 2 && text[1] == ':' ? 2 : 0;
    while(length < text.size() && grammar.separators.find(text[length]) != std::string_view::npos)
        ++length;
    return length;
}

inline bool rooted(std::string_view text, const path_grammar &grammar = native_paths)
{
    return root_length(text, grammar) != 0;
}

// A rooted relative part would replace the root it is joined onto rather than name a path under it.
inline std::optional<std::filesystem::path> join_under(const std::filesystem::path &root, std::string_view package, std::string_view relative,
                                                       const path_grammar &grammar = native_paths)
{
    if(rooted(relative, grammar))
        return std::nullopt;
    std::filesystem::path candidate = root / std::string(package);
    if(!relative.empty())
        candidate /= std::string(relative);
    return candidate;
}

}

#endif
