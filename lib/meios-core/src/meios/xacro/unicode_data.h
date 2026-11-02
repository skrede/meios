#ifndef HPP_GUARD_MEIOS_XACRO_UNICODE_DATA_H
#define HPP_GUARD_MEIOS_XACRO_UNICODE_DATA_H

#include <span>
#include <cstdint>

namespace meios::detail
{

struct code_point_range
{
    char32_t first;
    char32_t last;
};

struct combining_run
{
    char32_t first;
    char32_t last;
    std::uint8_t combining_class;
};

struct composition
{
    char32_t first;
    char32_t second;
    char32_t composite;
};

struct decomposition
{
    char32_t code_point;
    std::uint16_t offset;
    std::uint8_t length;
};

std::span<const code_point_range> xid_start_ranges();

// The code points outside ASCII that continue an identifier but cannot start one.
std::span<const code_point_range> xid_continue_ranges();

std::span<const combining_run> combining_runs();

std::span<const composition> compositions();

// The full compatibility decompositions of the code points that continue an identifier, Hangul
// aside, each a slice of the pool.
std::span<const decomposition> decompositions();

std::span<const char32_t> decomposition_pool();

}

#endif
