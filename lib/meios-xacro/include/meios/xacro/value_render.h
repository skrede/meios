#ifndef HPP_GUARD_MEIOS_XACRO_VALUE_RENDER_H
#define HPP_GUARD_MEIOS_XACRO_VALUE_RENDER_H

#include "meios/xacro/value.h"
#include "meios/xacro/detail/numeric.h"

#include <string>
#include <optional>
#include <string_view>

namespace meios
{

// A collection has no scalar spelling, so it yields no text at all rather than a
// serialized dump; the absent result is what turns a collection in mixed document text
// into a typed, located failure instead of invented XML.
inline std::optional<std::string> render_scalar(const value &subject)
{
    switch(subject.kind())
    {
        case value_kind::null:    return std::string("None");
        case value_kind::boolean: return *subject.boolean() ? "True" : "False";
        case value_kind::integer: return std::to_string(*subject.integer());
        case value_kind::real:    return detail::print_double(*subject.real());
        case value_kind::string:  return *subject.text();
        default:                  return std::nullopt;
    }
}

inline std::string_view kind_name(value_kind kind)
{
    switch(kind)
    {
        case value_kind::null:     return "null";
        case value_kind::boolean:  return "boolean";
        case value_kind::integer:  return "integer";
        case value_kind::real:     return "real";
        case value_kind::string:   return "string";
        case value_kind::sequence: return "sequence";
        default:                   return "mapping";
    }
}

}

#endif
