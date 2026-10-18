#ifndef HPP_GUARD_MEIOS_XACRO_INCLUDE_TARGET_H
#define HPP_GUARD_MEIOS_XACRO_INCLUDE_TARGET_H

#include <string>
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

std::string normalize_relative(std::string_view rel, bool &escaped);

std::filesystem::path include_key(const std::string &package, const std::string &normalized);

}

#endif
