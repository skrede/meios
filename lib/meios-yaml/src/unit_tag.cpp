#include "scalar_resolver.h"

#include "meios/xacro/value.h"
#include "meios/xacro/detail/numeric.h"

#include <string>
#include <numbers>
#include <optional>
#include <string_view>

namespace meios::detail
{
namespace
{

// xacro converts a tagged angle by multiplying by a constant it computes once, not by a library
// angle-conversion function. The two agree bit for bit on every operand measured here, but the
// multiplication is what reproduces upstream in general, and that is the claim being made.
constexpr double pi_over_180 = std::numbers::pi / 180.0;

// The tagged text is evaluated as a Python expression upstream. Only a numeric literal is
// accepted here; the character set refuses a name, a call or an operator before the text is
// ever read as a number, so an unmeasured spelling fails loudly instead of being approximated.
std::optional<double> numeric_literal(std::string_view text)
{
    if(text.empty() || text.find_first_not_of("+-.0123456789eE") != std::string_view::npos)
        return std::nullopt;
    std::string cleaned(text);
    if(cleaned.front() == '+')
        cleaned.erase(cleaned.begin());
    bool ok = false;
    const double number = parse_double(cleaned, ok);
    return ok ? std::optional<double>(number) : std::nullopt;
}

}

scalar_result resolve_unit_tag(std::string_view text, std::string_view tag)
{
    if(tag != "!degrees")
        return declines("an unsupported tag '" + std::string(tag) + '\'');
    const std::optional<double> literal = numeric_literal(text);
    if(!literal)
        return declines("a '!degrees' value that is not a finite numeric literal");
    const std::optional<value> converted = value::make_real(*literal * pi_over_180);
    if(!converted)
        return rejects("an angle that is not a finite number");
    return yields(*converted);
}

}
