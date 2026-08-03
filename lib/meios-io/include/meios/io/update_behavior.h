#ifndef HPP_GUARD_MEIOS_IO_UPDATE_BEHAVIOR_H
#define HPP_GUARD_MEIOS_IO_UPDATE_BEHAVIOR_H

#include <string_view>

namespace meios
{

enum class update_behavior
{
    reject,
    replace,
};

constexpr std::string_view to_string(update_behavior behavior) noexcept
{
    switch(behavior)
    {
        case update_behavior::reject:
            return "reject";
        case update_behavior::replace:
            return "replace";
    }
    return "unknown";
}

}

#endif
