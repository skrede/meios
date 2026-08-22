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
    create,
    permissions,
    write,
    publish,
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
        case operation_kind::create:
            return "create";
        case operation_kind::permissions:
            return "permissions";
        case operation_kind::write:
            return "write";
        case operation_kind::publish:
            return "publish";
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
