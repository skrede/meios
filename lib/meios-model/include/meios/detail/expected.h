#ifndef HPP_GUARD_MEIOS_MODEL_DETAIL_EXPECTED_H
#define HPP_GUARD_MEIOS_MODEL_DETAIL_EXPECTED_H

#include <version>

// MEIOS_EXPECTED_FORCE_FALLBACK selects the C++20 fallback even where the standard
// type exists, so both branches stay exercisable on a single toolchain.
#if !defined(MEIOS_EXPECTED_FORCE_FALLBACK) && defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L

    #include <expected>

namespace meios::detail
{

template <typename T, typename E>
using expected = std::expected<T, E>;

template <typename E>
using unexpected = std::unexpected<E>;

using unexpect_t = std::unexpect_t;
inline constexpr unexpect_t unexpect{std::unexpect};

}

#else

    #include "meios/detail/expected_fallback.h"

#endif

#endif
