#ifndef HPP_GUARD_MEIOS_MODEL_EXPECTED_H
#define HPP_GUARD_MEIOS_MODEL_EXPECTED_H

#include "meios/detail/expected.h"

namespace meios
{

template <typename T, typename E>
using expected = detail::expected<T, E>;

template <typename E>
using unexpected = detail::unexpected<E>;

using detail::unexpect_t;
using detail::unexpect;

}

#endif
