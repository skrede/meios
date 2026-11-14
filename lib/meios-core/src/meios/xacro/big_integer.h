#ifndef HPP_GUARD_MEIOS_XACRO_BIG_INTEGER_H
#define HPP_GUARD_MEIOS_XACRO_BIG_INTEGER_H

#include <string>
#include <vector>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace meios::detail
{

// A sign and a little-endian magnitude of 32-bit limbs with no high zero limb, so zero has no
// limbs and is never negative, and two equal integers hold equal members.
class big_integer
{
public:
    explicit big_integer(std::int64_t number);

    // An optional '-' and one or more decimal digits; nothing for any other text.
    static std::optional<big_integer> from_spelling(std::string_view text);

    std::string spelling() const;

    std::optional<std::int64_t> int64() const;

    bool negative() const { return m_negative; }

    std::size_t bit_length() const;

    std::strong_ordering operator<=>(const big_integer &other) const;

    bool operator==(const big_integer &other) const = default;

    friend big_integer operator-(const big_integer &a) { return big_integer(a.m_limbs, !a.m_negative); }

    friend big_integer abs(const big_integer &a) { return big_integer(a.m_limbs, false); }

    friend big_integer operator+(const big_integer &a, const big_integer &b) { return signed_sum(a, b, b.m_negative); }

    friend big_integer operator-(const big_integer &a, const big_integer &b) { return signed_sum(a, b, !b.m_negative); }

    friend big_integer operator*(const big_integer &a, const big_integer &b);
    friend big_integer pow(const big_integer &base, std::uint64_t exponent);

private:
    bool m_negative;
    std::vector<std::uint32_t> m_limbs;

    big_integer(std::vector<std::uint32_t> limbs, bool negative);

    static big_integer signed_sum(const big_integer &a, const big_integer &b, bool b_negative);
};

}

#endif
