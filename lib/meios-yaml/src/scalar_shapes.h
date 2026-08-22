#ifndef HPP_GUARD_MEIOS_YAML_SCALAR_SHAPES_H
#define HPP_GUARD_MEIOS_YAML_SCALAR_SHAPES_H

#include <cstdint>
#include <cstddef>
#include <optional>
#include <string_view>

namespace meios::detail
{

// The base an integer spelling is read in, with the offset of its first digit so a caller
// can skip a base prefix the conversion does not accept.
struct integer_shape
{
    std::int32_t base;
    std::size_t first_digit;
};

std::optional<bool> boolean_shape(std::string_view text);

bool null_shape(std::string_view text);

bool float_shape(std::string_view text);

bool non_finite_shape(std::string_view text);

bool sexagesimal_shape(std::string_view text);

std::optional<integer_shape> integer_spelling(std::string_view text);

}

#endif
