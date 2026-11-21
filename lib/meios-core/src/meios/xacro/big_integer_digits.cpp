#include "big_integer.h"

#include <vector>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <optional>
#include <string_view>

namespace meios::detail
{
namespace
{

std::optional<std::uint32_t> digit_value(char c, std::uint32_t base)
{
    std::uint32_t digit = base;
    if(c >= '0' && c <= '9') digit = static_cast<std::uint32_t>(c - '0');
    else if(c >= 'a' && c <= 'f') digit = static_cast<std::uint32_t>(c - 'a' + 10);
    else if(c >= 'A' && c <= 'F') digit = static_cast<std::uint32_t>(c - 'A' + 10);
    return digit < base ? std::optional<std::uint32_t>(digit) : std::nullopt;
}

}

std::optional<big_integer> big_integer::from_power_of_two_digits(std::string_view digits, std::uint32_t bits_per_digit)
{
    if(bits_per_digit == 0 || bits_per_digit > 4) return std::nullopt;
    const std::uint32_t base = std::uint32_t{ 1 } << bits_per_digit;
    std::vector<std::uint32_t> magnitude(digits.size() * bits_per_digit / 32 + 1, 0);
    std::size_t bit = 0;
    for(std::size_t i = digits.size(); i-- > 0; bit += bits_per_digit)
    {
        const std::optional<std::uint32_t> digit = digit_value(digits[i], base);
        if(!digit) return std::nullopt;
        const std::uint64_t placed = std::uint64_t{ *digit } << (bit % 32);
        magnitude[bit / 32] |= static_cast<std::uint32_t>(placed);
        if((placed >> 32) != 0) magnitude[bit / 32 + 1] |= static_cast<std::uint32_t>(placed >> 32);
    }
    return big_integer(std::move(magnitude), false);
}

}
