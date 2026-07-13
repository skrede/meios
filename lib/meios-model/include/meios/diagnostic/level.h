#ifndef HPP_GUARD_MEIOS_MODEL_DIAGNOSTIC_LEVEL_H
#define HPP_GUARD_MEIOS_MODEL_DIAGNOSTIC_LEVEL_H

#include <string_view>

namespace meios
{

enum class level
{
    error,
    warn,
    info,
};

constexpr std::string_view to_string(level lvl) noexcept
{
    switch(lvl)
    {
        case level::error: return "error";
        case level::warn:  return "warn";
        case level::info:  return "info";
    }
    return "unknown";
}

}

#endif
