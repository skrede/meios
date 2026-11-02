#ifndef HPP_GUARD_MEIOS_XACRO_NFKC_H
#define HPP_GUARD_MEIOS_XACRO_NFKC_H

#include <string>
#include <string_view>

namespace meios::detail
{

// Unicode normalization form KC, exact for a sequence of code points that each continue a Python
// identifier, which is all the decomposition table holds.
std::u32string nfkc(std::u32string_view points);

}

#endif
