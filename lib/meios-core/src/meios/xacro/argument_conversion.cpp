#include "python_space.h"
#include "python_number.h"
#include "argument_conversion.h"

#include "meios/xacro/value.h"
#include "meios/xacro/detail/numeric.h"

#include <cctype>
#include <string>
#include <cstdint>
#include <optional>
#include <algorithm>
#include <string_view>

namespace meios::detail
{

namespace
{

std::optional<value> integer_spelling(std::string_view text)
{
    const std::optional<std::string> digits = python_int(text);
    if(!digits)
        return value{std::string(text)};
    bool ok                   = false;
    const std::int64_t number = parse_int(*digits, ok);
    return ok ? std::optional<value>(value{number}) : std::nullopt;
}

std::optional<value> real_spelling(std::string_view text)
{
    const std::optional<double> number = python_real(text);
    return number ? value::make_real(*number) : std::optional<value>(value{std::string(text)});
}

std::string lowered(std::string_view text)
{
    std::string out(text);
    std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

}

std::optional<value> converted_argument(std::string_view text)
{
    const std::string_view number   = python_stripped(text);
    const std::optional<value> read = text.find('.') == std::string_view::npos ? integer_spelling(number) : real_spelling(number);
    if(!read || read->kind() != value_kind::string)
        return read;
    const std::string spelling = lowered(text);
    if(spelling == "true" || spelling == "false")
        return value{spelling == "true"};
    return value{std::string(text)};
}

}
