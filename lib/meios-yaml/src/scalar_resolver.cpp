#include "scalar_shapes.h"
#include "scalar_resolver.h"

#include "meios/xacro/value.h"
#include "meios/xacro/detail/numeric.h"

#include <string>
#include <cstdint>
#include <charconv>
#include <optional>
#include <algorithm>
#include <string_view>
#include <system_error>

namespace meios::detail
{
namespace
{

std::string without_separators(std::string_view text)
{
    std::string cleaned(text);
    std::erase(cleaned, '_');
    // from_chars accepts a minus sign and refuses a plus one, in every base.
    if(!cleaned.empty() && cleaned.front() == '+')
        cleaned.erase(cleaned.begin());
    return cleaned;
}

scalar_result real_from(std::string_view text)
{
    const std::string cleaned = without_separators(text);
    bool ok = false;
    const double number = parse_double(cleaned, ok);
    if(!ok)
        return rejects("a number this evaluator cannot read");
    const std::optional<value> made = value::make_real(number);
    if(!made)
        return rejects("a number that is not finite");
    return yields(*made);
}

scalar_result integer_from(std::string_view text, const integer_shape &shape)
{
    std::string cleaned = text[0] == '-' ? "-" : "";
    cleaned += without_separators(text.substr(shape.first_digit));
    std::int64_t number = 0;
    const char *last = cleaned.data() + cleaned.size();
    const std::from_chars_result read =
        std::from_chars(cleaned.data(), last, number, shape.base);
    if(read.ec != std::errc{} || read.ptr != last)
        return rejects("a number this evaluator cannot represent");
    return yields(value{ number });
}

// The classes and their order reproduce PyYAML's implicit resolver, which xacro loads
// auxiliary documents through. The order is load-bearing: the float class is tried before
// the integer one, and both are tried before a scalar becomes text.
scalar_result resolve_plain(std::string_view text)
{
    if(const std::optional<bool> flag = boolean_shape(text))
        return yields(value{ *flag });
    if(sexagesimal_shape(text))
        return declines("a sexagesimal number '" + std::string(text) + '\'');
    if(non_finite_shape(text))
        return rejects("a number that is not finite");
    if(float_shape(text))
        return real_from(text);
    if(const std::optional<integer_shape> shape = integer_spelling(text))
        return integer_from(text, *shape);
    if(null_shape(text))
        return yields(value{});
    return yields(value{ std::string(text) });
}

}

scalar_result resolve_scalar(std::string_view text, std::string_view tag)
{
    // Upstream never implicitly resolves a quoted, literal or folded scalar, and the
    // non-specific tag is the only place that distinction survives into the node stream.
    if(tag == "!")
        return yields(value{ std::string(text) });
    if(tag != "?")
        return resolve_unit_tag(text, tag);
    return resolve_plain(text);
}

}
