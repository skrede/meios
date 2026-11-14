#include "big_integer.h"

#include <bit>
#include <array>
#include <limits>
#include <string>
#include <vector>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <charconv>
#include <optional>
#include <algorithm>
#include <string_view>

namespace meios::detail
{

namespace
{

using limbs = std::vector<std::uint32_t>;

constexpr std::size_t chunk_digits = 9;
constexpr std::array<std::uint32_t, chunk_digits + 1> ten_to{ 1, 10, 100, 1'000, 10'000, 100'000, 1'000'000, 10'000'000, 100'000'000, 1'000'000'000 };

limbs trimmed(limbs magnitude)
{
    while(!magnitude.empty() && magnitude.back() == 0)
        magnitude.pop_back();
    return magnitude;
}

limbs magnitude_of(std::uint64_t number)
{
    limbs magnitude;
    for(; number != 0; number >>= 32)
        magnitude.push_back(static_cast<std::uint32_t>(number));
    return magnitude;
}

std::strong_ordering compare_magnitudes(const limbs &a, const limbs &b)
{
    if(a.size() != b.size())
        return a.size() <=> b.size();
    return std::lexicographical_compare_three_way(a.rbegin(), a.rend(), b.rbegin(), b.rend());
}

limbs add_magnitudes(const limbs &a, const limbs &b)
{
    const limbs &longer = a.size() >= b.size() ? a : b;
    const limbs &shorter = a.size() >= b.size() ? b : a;
    limbs sum(longer.size() + 1, 0);
    std::uint64_t carry = 0;
    for(std::size_t i = 0; i < longer.size(); ++i)
    {
        carry += std::uint64_t{ longer[i] } + (i < shorter.size() ? shorter[i] : 0u);
        sum[i] = static_cast<std::uint32_t>(carry);
        carry >>= 32;
    }
    sum.back() = static_cast<std::uint32_t>(carry);
    return sum;
}

limbs subtract_magnitudes(const limbs &larger, const limbs &smaller)
{
    limbs difference(larger.size(), 0);
    std::uint64_t borrow = 0;
    for(std::size_t i = 0; i < larger.size(); ++i)
    {
        const std::uint64_t taken = std::uint64_t{ i < smaller.size() ? smaller[i] : 0u } + borrow;
        difference[i] = static_cast<std::uint32_t>(larger[i] - taken);
        borrow = larger[i] < taken ? 1 : 0;
    }
    return difference;
}

void scale_and_add(limbs &magnitude, std::uint32_t factor, std::uint32_t addend)
{
    std::uint64_t carry = addend;
    for(std::uint32_t &limb : magnitude)
    {
        carry += std::uint64_t{ limb } * factor;
        limb = static_cast<std::uint32_t>(carry);
        carry >>= 32;
    }
    if(carry != 0)
        magnitude.push_back(static_cast<std::uint32_t>(carry));
}

std::uint32_t chunk_value(std::string_view digits)
{
    std::uint32_t value = 0;
    std::from_chars(digits.data(), digits.data() + digits.size(), value);
    return value;
}

}

std::uint32_t big_integer::divide_in_place(limbs &magnitude, std::uint32_t divisor)
{
    std::uint64_t remainder = 0;
    for(std::size_t i = magnitude.size(); i-- > 0;)
    {
        const std::uint64_t current = (remainder << 32) | magnitude[i];
        magnitude[i] = static_cast<std::uint32_t>(current / divisor);
        remainder = current % divisor;
    }
    magnitude = trimmed(std::move(magnitude));
    return static_cast<std::uint32_t>(remainder);
}

big_integer::big_integer(std::int64_t number)
        : m_negative(number < 0)
        , m_limbs(magnitude_of(number < 0 ? ~static_cast<std::uint64_t>(number) + 1 : static_cast<std::uint64_t>(number)))
{
}

big_integer::big_integer(std::vector<std::uint32_t> limbs, bool negative)
        : m_negative(negative && std::ranges::any_of(limbs, [](std::uint32_t limb) { return limb != 0; }))
        , m_limbs(trimmed(std::move(limbs)))
{
}

std::optional<big_integer> big_integer::from_spelling(std::string_view text)
{
    const bool negative = text.starts_with('-');
    const std::string_view digits = text.substr(negative ? 1 : 0);
    if(digits.empty() || digits.find_first_not_of("0123456789") != std::string_view::npos)
        return std::nullopt;
    limbs magnitude;
    std::size_t width = digits.size() % chunk_digits == 0 ? chunk_digits : digits.size() % chunk_digits;
    for(std::size_t at = 0; at < digits.size(); at += width, width = chunk_digits)
        scale_and_add(magnitude, ten_to[width], chunk_value(digits.substr(at, width)));
    return big_integer(std::move(magnitude), negative);
}

std::string big_integer::spelling() const
{
    limbs rest = m_limbs;
    std::vector<std::uint32_t> chunks;
    do
        chunks.push_back(divide_in_place(rest, ten_to[chunk_digits]));
    while(!rest.empty());
    std::string out = m_negative ? "-" : "";
    out += std::to_string(chunks.back());
    for(std::size_t i = chunks.size() - 1; i-- > 0;)
    {
        const std::string part = std::to_string(chunks[i]);
        out.append(chunk_digits - part.size(), '0').append(part);
    }
    return out;
}

std::optional<std::int64_t> big_integer::int64() const
{
    constexpr std::uint64_t edge = std::uint64_t{ 1 } << 63;
    if(m_limbs.size() > 2)
        return std::nullopt;
    std::uint64_t magnitude = 0;
    for(std::size_t i = m_limbs.size(); i-- > 0;)
        magnitude = (magnitude << 32) | m_limbs[i];
    if(m_negative ? magnitude > edge : magnitude >= edge)
        return std::nullopt;
    if(m_negative && magnitude == edge)
        return std::numeric_limits<std::int64_t>::min();
    const std::int64_t number = static_cast<std::int64_t>(magnitude);
    return m_negative ? -number : number;
}

std::size_t big_integer::bit_length() const
{
    if(m_limbs.empty())
        return 0;
    return (m_limbs.size() - 1) * 32 + static_cast<std::size_t>(std::bit_width(m_limbs.back()));
}

std::strong_ordering big_integer::operator<=>(const big_integer &other) const
{
    if(m_negative != other.m_negative)
        return m_negative ? std::strong_ordering::less : std::strong_ordering::greater;
    const std::strong_ordering magnitude = compare_magnitudes(m_limbs, other.m_limbs);
    return m_negative ? 0 <=> magnitude : magnitude;
}

big_integer big_integer::signed_sum(const big_integer &a, const big_integer &b, bool b_negative)
{
    if(a.m_negative == b_negative)
        return big_integer(add_magnitudes(a.m_limbs, b.m_limbs), b_negative);
    if(compare_magnitudes(a.m_limbs, b.m_limbs) >= 0)
        return big_integer(subtract_magnitudes(a.m_limbs, b.m_limbs), a.m_negative);
    return big_integer(subtract_magnitudes(b.m_limbs, a.m_limbs), b_negative);
}

}
