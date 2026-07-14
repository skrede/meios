#ifndef HPP_GUARD_MEIOS_BUNDLE_RESULT_H
#define HPP_GUARD_MEIOS_BUNDLE_RESULT_H

#include <cstddef>
#include <string_view>

namespace meios
{

enum class emit_status
{
    ok,
    xml_unencodable,
    malformed_reference,
    package_collision,
    io_error,
};

constexpr std::string_view to_string(emit_status status) noexcept
{
    switch(status)
    {
        case emit_status::ok:                  return "ok";
        case emit_status::xml_unencodable:     return "xml_unencodable";
        case emit_status::malformed_reference: return "malformed_reference";
        case emit_status::package_collision:   return "package_collision";
        case emit_status::io_error:            return "io_error";
    }
    return "unknown";
}

struct emit_result
{
    emit_status status;
    std::size_t assets_seen;
    std::size_t unresolved;
};

}

#endif
