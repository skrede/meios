#include "literal_read.h"

#include "meios/xacro/value.h"
#include "meios/xacro/structural.h"

#include "meios/xacro/detail/numeric.h"

#include <string>
#include <optional>
#include <string_view>

namespace meios::detail
{

// The number kinds are tried before the boolean spellings, so "1" stays an integer rather than a
// true; only these four spellings ever reach the boolean arm. A text an exact span yields is
// recovered this way so that it means what the same characters mean in mixed text. Digits past the
// integer digit ceiling stay text, because a forwarded integer must render as written, not as a
// rounded real.
value classify(std::string_view text)
{
    if(std::optional<value> integer = integer_literal(text))
        return *integer;
    bool ok = false;
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
