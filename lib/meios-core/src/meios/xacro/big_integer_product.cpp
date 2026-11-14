#include "big_integer.h"

#include <vector>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace meios::detail
{

// Schoolbook multiplication over 64-bit partial products: a limb product plus the limb already
// in place plus the carry is at most (2^32 - 1)^2 + 2 (2^32 - 1) = 2^64 - 1, so nothing is lost.
big_integer operator*(const big_integer &a, const big_integer &b)
{
    std::vector<std::uint32_t> product(a.m_limbs.size() + b.m_limbs.size(), 0);
    for(std::size_t i = 0; i < a.m_limbs.size(); ++i)
    {
        std::uint64_t carry = 0;
        for(std::size_t j = 0; j < b.m_limbs.size(); ++j)
        {
            carry += std::uint64_t{ a.m_limbs[i] } * b.m_limbs[j] + product[i + j];
            product[i + j] = static_cast<std::uint32_t>(carry);
            carry >>= 32;
        }
        product[i + b.m_limbs.size()] = static_cast<std::uint32_t>(carry);
    }
    return big_integer(std::move(product), a.m_negative != b.m_negative);
}

// Exponentiation by squaring (square and multiply), one squaring per bit of the exponent.
big_integer pow(const big_integer &base, std::uint64_t exponent)
{
    big_integer result{ std::int64_t{ 1 } };
    big_integer square = base;
    for(; exponent != 0; exponent >>= 1)
    {
        if((exponent & 1) != 0)
            result = result * square;
        if(exponent > 1)
            square = square * square;
    }
    return result;
}

}
