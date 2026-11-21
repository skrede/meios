#include "number_token.h"

#include "lexer.h"
#include "big_integer.h"
#include "number_scan.h"
#include "literal_read.h"

#include "meios/xacro/value.h"

#include "meios/xacro/detail/numeric.h"
#include "meios/xacro/detail/wide_integer.h"

#include <bit>
#include <array>
#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <algorithm>
#include <string_view>

namespace meios::detail
{
namespace
{

// Past this bit length no integer has wide_digit_ceiling or fewer decimal digits.
constexpr std::size_t widest_bits = 14285;

std::string without_separators(std::string_view text)
{
    std::string digits(text);
    std::erase(digits, '_');
    return digits;
}

bool leading_zero(std::string_view digits)
{
    return digits.size() > 1 && digits.front() == '0' && digits.find_first_not_of('0') != std::string_view::npos;
}

std::optional<value> decimal_value(const number_scan &s, std::string_view lexeme)
{
    const std::string digits = without_separators(lexeme);
    if(!s.real)
        return leading_zero(digits) ? std::nullopt : integer_literal(digits);
    bool ok = false;
    const double number = parse_double(digits, ok);
    return ok ? value::make_real(number) : std::nullopt;
}

std::size_t significant_bits(std::string_view digits, std::uint32_t bits_per_digit)
{
    if(digits.empty()) return 0;
    const char lead = digits.front();
    const auto leading = static_cast<std::uint32_t>(decimal_digit(lead) ? lead - '0' : (lead | 0x20) - 'a' + 10);
    return (digits.size() - 1) * bits_per_digit + static_cast<std::size_t>(std::bit_width(leading));
}

std::optional<value> prefixed_value(std::string_view lexeme, std::uint32_t base)
{
    const auto bits_per_digit = static_cast<std::uint32_t>(std::bit_width(base - 1));
    std::string digits = without_separators(lexeme.substr(2));
    digits.erase(0, std::min(digits.find_first_not_of('0'), digits.size()));
    if(significant_bits(digits, bits_per_digit) > widest_bits) return std::nullopt;
    const std::optional<big_integer> number = big_integer::from_power_of_two_digits(digits, bits_per_digit);
    if(!number) return std::nullopt;
    if(const std::optional<std::int64_t> small = number->int64()) return value{ *small };
    const std::optional<wide_integer> wide = wide_integer::from_spelling(number->spelling());
    return wide ? std::optional<value>(value{ *wide }) : std::nullopt;
}

bool word_character(char c)
{
    return decimal_digit(c) || ((c | 0x20) >= 'a' && (c | 0x20) <= 'z') || c == '_';
}

// A literal may touch one of these keywords (`1if 1else 2`); any other name directly after it makes
// the literal malformed.
constexpr std::array<std::string_view, 8> adjacent_keywords = { "and", "else", "for", "if", "in", "is", "not", "or" };

bool followed_by_name(std::string_view src, std::size_t at)
{
    const std::string_view rest = src.substr(at);
    const auto starts_rest = [rest](std::string_view keyword) { return rest.starts_with(keyword); };
    return !rest.empty() && word_character(rest.front()) && std::ranges::none_of(adjacent_keywords, starts_rest);
}

// A malformed literal leaves no point to resume reading from, so its token ends the stream.
std::size_t push_malformed(std::string_view src, std::size_t i, std::size_t at, std::vector<token> &out)
{
    while(at < src.size() && word_character(src[at])) ++at;
    out.push_back(token{ token_kind::error, src.substr(i, at - i) });
    return src.size();
}

bool starts_number(std::string_view text)
{
    return !text.empty() && (decimal_digit(text.front()) || (text.front() == '.' && text.size() > 1 && decimal_digit(text[1])));
}

std::string flaw_message(const number_scan &s, std::string_view text)
{
    const std::string quoted = ": '" + std::string(text) + "'";
    if(s.fault == number_flaw::separator) return "a digit separator '_' in a number stands only between two digits" + quoted;
    if(s.fault == number_flaw::exponent) return "the exponent of a number has no digit" + quoted;
    if(s.fault == number_flaw::no_digit) return "the prefix '" + std::string(text.substr(0, 2)) + "' of an integer literal is followed by no digit" + quoted;
    if(s.base == 2) return "a binary integer literal holds only the digits 0 and 1" + quoted;
    return "an octal integer literal holds only the digits 0 to 7" + quoted;
}

}

std::size_t push_number(std::string_view src, std::size_t i, std::vector<token> &out)
{
    const number_scan s = read_number(src, i);
    if(s.fault != number_flaw::none || followed_by_name(src, s.at)) return push_malformed(src, i, s.at, out);
    const std::string_view lexeme = src.substr(i, s.at - i);
    if(s.imaginary) { out.push_back(token{ token_kind::unsupported, lexeme }); return s.at; }
    const std::optional<value> leaf = s.base == 10 ? decimal_value(s, lexeme) : prefixed_value(lexeme, s.base);
    out.push_back(token{ leaf ? token_kind::number : token_kind::error, lexeme, leaf.value_or(value{}) });
    return s.at;
}

// Beyond a flaw, declined are a literal a name follows directly, an integer whose zero leads a nonzero
// decimal one or whose value is past the ceiling, and a real a double cannot hold.
std::optional<std::string> number_fault(std::string_view text)
{
    if(!starts_number(text)) return std::nullopt;
    const number_scan s = read_number(text, 0);
    const std::string ceiling = std::to_string(wide_digit_ceiling);
    if(s.fault != number_flaw::none) return flaw_message(s, text);
    if(s.at < text.size()) return "a number is directly followed by a name: '" + std::string(text) + "'";
    if(s.imaginary) return std::nullopt;
    if(s.real) return "a real literal is outside the range of a double: '" + std::string(text) + "'";
    if(s.base != 10) return "an integer literal's value has more than the " + ceiling + " decimal digits an integer holds here";
    if(leading_zero(without_separators(text)))
        return "an integer literal other than zero is written without a leading zero: '" + std::string(text) + "'";
    return "an integer literal has more than the " + ceiling + " digits an integer holds here";
}

bool imaginary_literal(std::string_view text)
{
    return starts_number(text) && (text.back() == 'j' || text.back() == 'J');
}

}
