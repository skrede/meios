#include "number_scan.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace meios::detail
{
namespace
{

bool digit_of(char c, std::uint32_t base)
{
    if(base == 16)
        return decimal_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    return c >= '0' && static_cast<std::uint32_t>(c - '0') < base;
}

char ahead(const number_scan &s, std::size_t offset = 0)
{
    return s.at + offset < s.src.size() ? s.src[s.at + offset] : '\0';
}

std::uint32_t prefix_base(std::string_view src, std::size_t i)
{
    const char marker = src[i] == '0' && i + 1 < src.size() ? src[i + 1] : '\0';
    if(marker == 'x' || marker == 'X')
        return 16;
    if(marker == 'o' || marker == 'O')
        return 8;
    if(marker == 'b' || marker == 'B')
        return 2;
    return 10;
}

// digit (["_"] digit)*, from a digit of the base under the cursor.
void read_digits(number_scan &s)
{
    for(++s.at;;)
    {
        const bool separated = ahead(s) == '_';
        const char next      = ahead(s, separated ? 1 : 0);
        if(digit_of(next, s.base))
        {
            s.at += separated ? 2 : 1;
            continue;
        }
        if(separated || decimal_digit(next))
            s.fault = decimal_digit(next) ? number_flaw::outside_base : number_flaw::separator;
        return;
    }
}

void read_prefixed(number_scan &s)
{
    s.at += 2;
    const bool separated = ahead(s) == '_';
    const char first     = ahead(s, separated ? 1 : 0);
    if(digit_of(first, s.base))
    {
        s.at += separated ? 1 : 0;
        read_digits(s);
    }
    else if(decimal_digit(first))
        s.fault = number_flaw::outside_base;
    else if(first == '_')
        s.fault = number_flaw::separator;
    else
        s.fault = number_flaw::no_digit;
}

void read_fraction(number_scan &s)
{
    s.real = true;
    ++s.at;
    if(decimal_digit(ahead(s)))
        read_digits(s);
    else if(ahead(s) == '_')
        s.fault = number_flaw::separator;
}

// An 'e' that neither a digit nor a signed digit follows is not part of the number, so `1else`
// reads as a number and a keyword.
void read_exponent(number_scan &s)
{
    const bool sign  = ahead(s, 1) == '+' || ahead(s, 1) == '-';
    const char first = ahead(s, sign ? 2 : 1);
    if(decimal_digit(first))
    {
        s.real = true;
        s.at += sign ? 2 : 1;
        read_digits(s);
    }
    else if(sign)
    {
        s.fault = number_flaw::exponent;
        s.at += 2;
    }
    else if(first == '_')
        s.fault = number_flaw::separator;
}

}

// Python 3 reference, "Lexical analysis", "Numeric literals"
// (docs.python.org/3/reference/lexical_analysis.html#numeric-literals).
number_scan read_number(std::string_view src, std::size_t i)
{
    number_scan s{.src = src, .at = i, .base = prefix_base(src, i), .fault = number_flaw::none, .real = false, .imaginary = false};
    if(s.base != 10)
    {
        read_prefixed(s);
        return s;
    }
    if(src[i] != '.')
        read_digits(s);
    if(s.fault == number_flaw::none && ahead(s) == '.')
        read_fraction(s);
    if(s.fault == number_flaw::none && (ahead(s) == 'e' || ahead(s) == 'E'))
        read_exponent(s);
    if(s.fault == number_flaw::none && (ahead(s) == 'j' || ahead(s) == 'J'))
    {
        s.imaginary = true;
        ++s.at;
    }
    return s;
}

}
