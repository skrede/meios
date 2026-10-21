#ifndef HPP_GUARD_MEIOS_XACRO_SUBSTITUTION_FIND_H
#define HPP_GUARD_MEIOS_XACRO_SUBSTITUTION_FIND_H

#include <string>
#include <vector>
#include <cstddef>
#include <optional>
#include <string_view>

namespace meios::detail
{

struct subst_ctx;

// A byte-served package renders no directory, so its directory is empty.
struct found_package
{
    std::string package;
    std::string directory;
    bool byte_served;
};

struct include_record
{
    std::vector<found_package> found;
    std::size_t span_offset;
};

std::optional<std::string> cmd_find(subst_ctx &ctx, std::string_view rest);

std::string names_no_directory(std::string_view package);

}

#endif
