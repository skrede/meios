#include "eval_numeric_ops.h"

#include <limits>
#include <cstdint>
#include <optional>

namespace meios::detail
{

namespace
{

constexpr std::int64_t lowest = std::numeric_limits<std::int64_t>::min();
constexpr std::int64_t highest = std::numeric_limits<std::int64_t>::max();

bool scale_by(std::int64_t &accumulator, std::int64_t factor)
{
    const std::optional<std::int64_t> made = checked_mul(accumulator, factor);
    if(!made)
        return false;
    accumulator = *made;
    return true;
}

}

std::optional<std::int64_t> checked_add(std::int64_t a, std::int64_t b)
{
    if(b > 0 ? a > highest - b : a < lowest - b)
        return std::nullopt;
    return a + b;
}

std::optional<std::int64_t> checked_sub(std::int64_t a, std::int64_t b)
{
    if(b < 0 ? a > highest + b : a < lowest + b)
        return std::nullopt;
    return a - b;
}

std::optional<std::int64_t> checked_mul(std::int64_t a, std::int64_t b)
{
    if(a > 0 && (b > 0 ? a > highest / b : b < lowest / a))
        return std::nullopt;
    if(a < 0 && (b > 0 ? a < lowest / b : b < highest / a))
        return std::nullopt;
    return a * b;
}

std::optional<std::int64_t> checked_negate(std::int64_t a)
{
    if(a == lowest)
        return std::nullopt;
    return -a;
}

std::optional<std::int64_t> checked_abs(std::int64_t a)
{
    if(a == lowest)
        return std::nullopt;
    return a < 0 ? -a : a;
}

// The most negative representable integer over minus one is the one quotient with no
// representable value, and on x86 the divide instruction raises SIGFPE for it rather than
// wrapping, so it is singled out here before the instruction can run.
std::optional<std::int64_t> checked_floordiv(std::int64_t a, std::int64_t b)
{
    if(b == 0 || (a == lowest && b == -1))
        return std::nullopt;
    std::int64_t quotient = a / b, remainder = a % b;
    if(remainder != 0 && ((remainder < 0) != (b < 0))) --quotient;
    return quotient;
}

// The remainder comes off the same instruction, so it raises on the same pair of operands.
std::optional<std::int64_t> checked_mod(std::int64_t a, std::int64_t b)
{
    if(b == 0 || (a == lowest && b == -1))
        return std::nullopt;
    std::int64_t remainder = a % b;
    if(remainder != 0 && ((remainder < 0) != (b < 0))) remainder += b;
    return remainder;
}

std::optional<std::int64_t> checked_power(std::int64_t base, std::int64_t exponent)
{
    std::int64_t result = 1;
    while(exponent > 0)
    {
        if((exponent & 1) && !scale_by(result, base))
            return std::nullopt;
        exponent >>= 1;
        if(exponent > 0 && !scale_by(base, base))
            return std::nullopt;
    }
    return result;
}

}
