#ifndef HPP_GUARD_MEIOS_MODEL_DIAGNOSTIC_OPERATION_FAILURE_H
#define HPP_GUARD_MEIOS_MODEL_DIAGNOSTIC_OPERATION_FAILURE_H

#include <string_view>
#include <system_error>

namespace meios
{

enum class operation_kind
{
    status,
    canonicalize,
    open,
    read,
    close,
};

constexpr std::string_view to_string(operation_kind operation) noexcept
{
    switch(operation)
    {
        case operation_kind::status:
            return "status";
        case operation_kind::canonicalize:
            return "canonicalize";
        case operation_kind::open:
            return "open";
        case operation_kind::read:
            return "read";
        case operation_kind::close:
            return "close";
    }
    return "unknown";
}

struct operation_failure
{
    operation_kind operation;
    std::error_code native;
};

}

#endif
