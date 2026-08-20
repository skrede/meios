#ifndef HPP_GUARD_MEIOS_XACRO_VALUE_RENDER_H
#define HPP_GUARD_MEIOS_XACRO_VALUE_RENDER_H

#include "meios/xacro/value.h"

#include "meios/xacro/detail/numeric.h"
#include "meios/xacro/detail/value_key.h"

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

// A key is always one of the five scalar kinds, so it always has a spelling; the value
// overload's absent result exists for a collection, which a key cannot hold.
inline std::string render_scalar(const detail::scalar_key &key)
{
    switch(key.kind())
    {
        case value_kind::null:    return std::string("None");
        case value_kind::boolean: return *key.boolean() ? "True" : "False";
        case value_kind::integer: return std::to_string(*key.integer());
        case value_kind::real:    return detail::print_double(*key.real());
        default:                  return *key.text();
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
