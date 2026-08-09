#ifndef HPP_GUARD_MEIOS_XACRO_DETAIL_NUMERIC_H
#define HPP_GUARD_MEIOS_XACRO_DETAIL_NUMERIC_H

#include <string>
#include <cstdint>
#include <string_view>

namespace meios::detail
{

double parse_double(std::string_view text, bool &ok);

std::int64_t parse_int(std::string_view text, bool &ok);

std::string print_double(double number);

}

#endif
