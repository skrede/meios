#include "literal_read.h"
#include "python_number.h"
#include "argument_conversion.h"

#include "meios/xacro/value.h"

#include <cctype>
#include <string>
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
    return integer_literal(*digits);
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
    const std::optional<value> read = text.find('.') == std::string_view::npos ? integer_spelling(text) : real_spelling(text);
    if(!read || read->kind() != value_kind::string)
        return read;
    const std::string spelling = lowered(text);
    if(spelling == "true" || spelling == "false")
        return value{spelling == "true"};
    return value{std::string(text)};
}

}
