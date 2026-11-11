#include "meios/xacro/value.h"
#include "meios/xacro/structural.h"
#include "meios/xacro/detail/numeric.h"

#include <string>
#include <cstdint>
#include <optional>
#include <string_view>

namespace meios::detail
{

// The number kinds are tried before the boolean spellings, so "1" stays an integer rather than a
// true; only these four spellings ever reach the boolean arm. A text an exact span yields is
// recovered this way so that it means what the same characters mean in mixed text.
value classify(std::string_view text)
{
    bool ok = false;
    const std::int64_t integer = parse_int(text, ok);
    if(ok)
        return value{ integer };
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
