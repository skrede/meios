#include "big_integer.h"

#include <bit>
#include <cmath>
#include <limits>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <algorithm>

namespace meios::detail
{

namespace
{

constexpr std::int64_t mantissa_bits = std::numeric_limits<double>::digits;
constexpr std::int64_t lowest_exponent = std::numeric_limits<double>::min_exponent;
constexpr std::int64_t highest_exponent = std::numeric_limits<double>::max_exponent;

// Rounds to nearest, ties to even, at the bit the kept precision ends on; the bits below it
// include a sticky bit, so a tie is never mistaken for a value above it.
std::uint64_t rounded(std::uint64_t bits, std::int64_t dropped)
{
    const std::uint64_t half = std::uint64_t{ 1 } << (dropped - 1);
    if((bits & half) != 0 && (bits & (3 * half - 1)) != 0)
        bits += half;
    return bits & ~(2 * half - 1);
}

std::int64_t bit_length(const big_integer &number)
{
    return static_cast<std::int64_t>(number.bit_length());
}

}

// The method of CPython's long_true_divide (Objects/longobject.c): the integer quotient is scaled to
// at least two bits beyond a double's precision, a nonzero remainder sets its lowest bit, and below
// the normal range the precision kept is the subnormal one, so the quotient is rounded only once.
std::optional<double> true_quotient(const big_integer &dividend, const big_integer &divisor)
{
    const double sign = dividend.negative() != divisor.negative() ? -1.0 : 1.0;
    const std::int64_t difference = bit_length(dividend) - bit_length(divisor);
    if(dividend.bit_length() == 0 || difference < lowest_exponent - mantissa_bits - 1)
        return std::copysign(0.0, sign);
    if(difference > highest_exponent)
        return std::nullopt;
    const std::int64_t shift = std::max(difference, lowest_exponent) - mantissa_bits - 2;
    const big_integer scaled = abs(dividend) << static_cast<std::size_t>(std::max<std::int64_t>(-shift, 0));
    const auto [quotient, remainder] = floor_divmod(scaled, abs(divisor) << static_cast<std::size_t>(std::max<std::int64_t>(shift, 0)));
    const std::uint64_t bits = static_cast<std::uint64_t>(*quotient.int64()) | (remainder.bit_length() != 0 ? 1u : 0u);
    const std::int64_t width = static_cast<std::int64_t>(std::bit_width(bits));
    const std::uint64_t kept = rounded(bits, std::max(width, lowest_exponent - shift) - mantissa_bits);
    const double magnitude = std::ldexp(static_cast<double>(kept), static_cast<int>(shift));
    if(!std::isfinite(magnitude))
        return std::nullopt;
    return std::copysign(magnitude, sign);
}

}
