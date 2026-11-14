#include "big_integer.h"

#include <bit>
#include <tuple>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace meios::detail
{

namespace
{

using limbs = std::vector<std::uint32_t>;

constexpr std::uint64_t limb_base = std::uint64_t{ 1 } << 32;

// One limb longer than the magnitude, the top limb holding the bits shifted out of it.
limbs shifted_left(const limbs &magnitude, std::size_t bits)
{
    limbs shifted(bits / 32, 0);
    const std::size_t within = bits % 32;
    std::uint32_t carry = 0;
    for(const std::uint32_t limb : magnitude)
    {
        const std::uint64_t moved = std::uint64_t{ limb } << within;
        shifted.push_back(static_cast<std::uint32_t>(moved) | carry);
        carry = static_cast<std::uint32_t>(moved >> 32);
    }
    shifted.push_back(carry);
    return shifted;
}

limbs shifted_right(const limbs &magnitude, std::size_t bits)
{
    limbs shifted(magnitude.size(), 0);
    for(std::size_t i = 0; i < magnitude.size(); ++i)
    {
        const std::uint64_t high = i + 1 < magnitude.size() ? magnitude[i + 1] : 0u;
        shifted[i] = static_cast<std::uint32_t>(((high << 32) | magnitude[i]) >> bits);
    }
    return shifted;
}

// Knuth's Algorithm D (TAOCP Vol. 2, section 4.3.1), steps D3 to D6, in the 32-bit form of
// Hacker's Delight's divmnu64 (2nd ed., section 9-2): over a normalized divisor the estimate from
// the top two limbs is at most two too large, the divisor's second limb corrects it at most twice,
// and an estimate still one too large shows as a borrow out of the subtraction.
std::uint64_t estimate(const limbs &u, const limbs &v, std::size_t j)
{
    const std::size_t n = v.size();
    const std::uint64_t top = (std::uint64_t{ u[j + n] } << 32) | u[j + n - 1];
    std::uint64_t qhat = top / v[n - 1], rhat = top % v[n - 1];
    while(qhat >= limb_base || qhat * v[n - 2] > ((rhat << 32) | u[j + n - 2]))
    {
        --qhat;
        rhat += v[n - 1];
        if(rhat >= limb_base)
            break;
    }
    return qhat;
}

bool multiply_subtract(limbs &u, const limbs &v, std::size_t j, std::uint64_t qhat)
{
    std::int64_t borrow = 0;
    for(std::size_t i = 0; i < v.size(); ++i)
    {
        const std::uint64_t product = qhat * v[i];
        const std::int64_t t = std::int64_t{ u[i + j] } - borrow - static_cast<std::int64_t>(product & 0xffffffffu);
        u[i + j] = static_cast<std::uint32_t>(t);
        borrow = static_cast<std::int64_t>(product >> 32) - (t >> 32);
    }
    const std::int64_t top = std::int64_t{ u[j + v.size()] } - borrow;
    u[j + v.size()] = static_cast<std::uint32_t>(top);
    return top < 0;
}

void add_back(limbs &u, const limbs &v, std::size_t j)
{
    std::uint64_t carry = 0;
    for(std::size_t i = 0; i < v.size(); ++i)
    {
        carry += std::uint64_t{ u[i + j] } + v[i];
        u[i + j] = static_cast<std::uint32_t>(carry);
        carry >>= 32;
    }
    u[j + v.size()] += static_cast<std::uint32_t>(carry);
}

// The dividend has at least as many limbs as the divisor, which has at least two.
std::pair<limbs, limbs> long_divide(const limbs &dividend, const limbs &divisor)
{
    const auto normalization = static_cast<std::size_t>(std::countl_zero(divisor.back()));
    limbs v = shifted_left(divisor, normalization);
    v.pop_back();
    limbs u = shifted_left(dividend, normalization);
    limbs quotient(dividend.size() - divisor.size() + 1, 0);
    for(std::size_t j = quotient.size(); j-- > 0;)
    {
        const std::uint64_t qhat = estimate(u, v, j);
        quotient[j] = static_cast<std::uint32_t>(qhat);
        if(multiply_subtract(u, v, j, qhat))
        {
            --quotient[j];
            add_back(u, v, j);
        }
    }
    u.resize(v.size());
    return { std::move(quotient), shifted_right(u, normalization) };
}

}

big_integer operator<<(const big_integer &a, std::size_t bits)
{
    return big_integer(shifted_left(a.m_limbs, bits), a.m_negative);
}

std::pair<big_integer, big_integer> floor_divmod(const big_integer &dividend, const big_integer &divisor)
{
    limbs quotient, remainder = dividend.m_limbs;
    if(divisor.m_limbs.size() == 1)
    {
        quotient = remainder;
        remainder = { big_integer::divide_in_place(quotient, divisor.m_limbs[0]) };
    }
    else if(remainder.size() >= divisor.m_limbs.size())
        std::tie(quotient, remainder) = long_divide(dividend.m_limbs, divisor.m_limbs);
    big_integer q(std::move(quotient), dividend.m_negative != divisor.m_negative);
    big_integer r(std::move(remainder), dividend.m_negative);
    if(r.m_limbs.empty() || dividend.m_negative == divisor.m_negative)
        return { std::move(q), std::move(r) };
    return { q - big_integer{ std::int64_t{ 1 } }, r + divisor };
}

}
