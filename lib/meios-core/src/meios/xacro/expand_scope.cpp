#include "meios/xacro/value.h"
#include "meios/xacro/structural.h"
#include "meios/xacro/detail/numeric.h"

#include <string>
#include <cstdint>
#include <charconv>
#include <optional>
#include <string_view>
#include <system_error>

namespace meios::detail
{

namespace
{

bool too_wide_integer(std::string_view text)
{
    std::int64_t unused               = 0;
    const char *last                  = text.data() + text.size();
    const std::from_chars_result read = std::from_chars(text.data(), last, unused);
    return read.ec == std::errc::result_out_of_range && read.ptr == last;
}

}

// The number kinds are tried before the boolean spellings, so "1" stays an integer rather than a
// true; only these four spellings ever reach the boolean arm. A text an exact span yields is
// recovered this way so that it means what the same characters mean in mixed text. Digits too wide
// for 64 bits stay text, because a forwarded integer must render as written, not as a rounded real.
value classify(std::string_view text)
{
    bool ok = false;
    const std::int64_t integer = parse_int(text, ok);
    if(ok)
        return value{ integer };
    if(too_wide_integer(text))
        return value{ std::string(text) };
    const double real = parse_double(text, ok);
    if(ok)
        if(std::optional<value> number = value::make_real(real))
            return *number;
    if(text == "true" || text == "True")
        return value{ true };
    if(text == "false" || text == "False")
        return value{ false };
    return value{ std::string(text) };
}

}
