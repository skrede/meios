#ifndef HPP_GUARD_MEIOS_XACRO_NUMBER_SCAN_H
#define HPP_GUARD_MEIOS_XACRO_NUMBER_SCAN_H

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace meios::detail
{

enum class number_flaw
{
    none,
    separator,
    no_digit,
    outside_base,
    exponent,
};

// One numeric literal read from src: it ends at `at`, or a flaw stops the read at the character that
// makes it malformed; base is 10 unless the literal is prefixed.
struct number_scan
{
    std::string_view src;
    std::size_t at;
    std::uint32_t base;
    number_flaw fault;
    bool real;
    bool imaginary;
};

inline bool decimal_digit(char c)
{
    return c >= '0' && c <= '9';
}

number_scan read_number(std::string_view src, std::size_t i);

}

#endif
