#include "boolean_value.h"

#include <string>
#include <cstddef>
#include <optional>
#include <algorithm>
#include <string_view>

namespace meios::detail
{

namespace
{

bool ascii_digit(char c)
{
    return c >= '0' && c <= '9';
}

// The ASCII characters int() strips from both ends: Python's ASCII whitespace, without the
// separators 0x1c-0x1f that str.isspace() also accepts.
std::string_view strip_python_space(std::string_view text)
{
    constexpr std::string_view space = " \t\n\v\f\r";
    const std::size_t first = text.find_first_not_of(space);
    if(first == std::string_view::npos)
        return {};
    return text.substr(first, text.find_last_not_of(space) - first + 1);
}

// Digits with single underscores between them, the underscores dropped.
std::optional<std::string> separated_digits(std::string_view text)
{
    std::string digits;
    for(std::size_t i = 0; i < text.size(); ++i)
    {
        const bool separator = text[i] == '_' && i > 0 && i + 1 < text.size()
                               && ascii_digit(text[i - 1]) && ascii_digit(text[i + 1]);
        if(!ascii_digit(text[i]) && !separator)
            return std::nullopt;
        if(!separator)
            digits += text[i];
    }
    if(digits.empty())
        return std::nullopt;
    return digits;
}

std::optional<bool> spelled_boolean(std::string_view text)
{
    if(text == "true" || text == "True")
        return true;
    if(text == "false" || text == "False")
        return false;
    const std::optional<std::string> number = python_int(text);
    if(!number)
        return std::nullopt;
    return *number != "0";
}

}

std::optional<std::string> python_int(std::string_view text)
{
    text = strip_python_space(text);
    const bool negative = text.starts_with('-');
    if(negative || text.starts_with('+'))
        text.remove_prefix(1);
    std::optional<std::string> digits = separated_digits(text);
    if(!digits)
        return std::nullopt;
    digits->erase(0, std::min(digits->find_first_not_of('0'), digits->size() - 1));
    if(negative && *digits != "0")
        digits->insert(0, 1, '-');
    return digits;
}

std::optional<bool> get_boolean_value(const value &v)
{
    switch(v.kind())
    {
        case value_kind::null:     return false;
        case value_kind::boolean:  return *v.boolean();
        case value_kind::integer:  return *v.integer() != 0;
        case value_kind::real:     return *v.real() != 0.0;
        case value_kind::string:   return spelled_boolean(*v.text());
        case value_kind::sequence:
        case value_kind::mapping:  return v.size() != 0;
    }
    return std::nullopt;
}

}
