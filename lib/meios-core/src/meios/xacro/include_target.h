#ifndef HPP_GUARD_MEIOS_XACRO_INCLUDE_TARGET_H
#define HPP_GUARD_MEIOS_XACRO_INCLUDE_TARGET_H

#include "meios/io/path_grammar.h"

#include <string>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

struct include_target
{
    std::string package;
    std::string relative;
};

include_target split_target(std::string_view raw);

bool is_separator(char c);

// The end of the leading elements of `text` that are exactly `directory`'s, with `.` and empty elements
// dropped and `..` kept; empty when they differ or one is rooted under the grammar and the other is not.
std::optional<std::size_t> directory_prefix(std::string_view text, std::string_view directory, const path_grammar &grammar = native_paths);

// Never rooted under the grammar: the spelling's root is removed before `..` is counted, and a drive
// the walk leaves leading is removed after it.
std::string normalize_relative(std::string_view rel, bool &escaped, const path_grammar &grammar = native_paths);

// The key is the file found, never the spelling that found it.
std::filesystem::path located_key(const std::filesystem::path &located);

struct include_lookups
{
    bool beside;
    std::optional<std::string> served_package;
};

std::string unresolved_message(const std::string &package, const std::string &normalized, const std::string &written, const include_lookups &asked);

}

#endif
