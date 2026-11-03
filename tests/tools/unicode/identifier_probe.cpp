#include "meios/detail/utf8.h"
#include "meios/xacro/identifier_text.h"

#include <string>
#include <cstddef>
#include <cstdint>
#include <charconv>
#include <iostream>
#include <iterator>
#include <optional>
#include <algorithm>
#include <string_view>

namespace
{

std::string spelled(std::string_view line)
{
    std::string text;
    const char *at  = line.data();
    const char *end = line.data() + line.size();
    while(at < end)
    {
        std::uint32_t point = 0;
        at                  = std::from_chars(at, end, point, 16).ptr + 1;
        meios::detail::append_utf8(text, static_cast<char32_t>(point));
    }
    return text;
}

void append_hex(std::string &out, char32_t point)
{
    char digits[8];
    const std::to_chars_result written = std::to_chars(digits, digits + sizeof(digits), static_cast<std::uint32_t>(point), 16);
    std::transform(digits, written.ptr, std::back_inserter(out), [](char c) { return c >= 'a' ? static_cast<char>(c - 'a' + 'A') : c; });
}

std::string hex_points(const std::optional<std::string> &text)
{
    if(!text)
        return "-";
    std::string out;
    for(std::size_t at = 0; at < text->size();)
    {
        const meios::detail::utf8_point one = meios::detail::decode_utf8(*text, at);
        if(at != 0)
            out += ' ';
        append_hex(out, one.value);
        at += one.length;
    }
    return out;
}

}

int main()
{
    std::ios::sync_with_stdio(false);
    std::string line;
    while(std::getline(std::cin, line))
    {
        const std::string text = spelled(line);
        std::cout << hex_points(meios::detail::python_name(text)) << '\t' << (meios::detail::valid_python_name(text) ? '1' : '0') << '\t'
                  << meios::detail::identifier_fault(text).value_or("-") << '\n';
    }
    return 0;
}
