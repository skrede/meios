#ifndef HPP_GUARD_MEIOS_XACRO_INCLUDE_TARGET_H
#define HPP_GUARD_MEIOS_XACRO_INCLUDE_TARGET_H

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
    std::optional<std::size_t> find_close;
};

include_target split_target(std::string_view raw);

std::string normalize_relative(std::string_view rel, bool &escaped);

// The key is the file found, never the spelling that found it.
std::filesystem::path located_key(const std::filesystem::path &located);

std::string unresolved_message(const std::string &package, const std::string &normalized, const std::string &written, bool looked_beside);

}

#endif
