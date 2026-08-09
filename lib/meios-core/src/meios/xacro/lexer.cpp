#include "lexer.h"

#include "meios/xacro/value.h"
#include "meios/xacro/detail/numeric.h"

#include <cctype>
#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace meios::detail
{
namespace
{

bool is_name_start(char c)
{
    return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_';
}

bool is_name_char(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

token classify_name(std::string_view word)
{
    if(word == "True")  return token{ token_kind::kw_true, word };
    if(word == "False") return token{ token_kind::kw_false, word };
    if(word == "and")   return token{ token_kind::kw_and, word };
    if(word == "or")    return token{ token_kind::kw_or, word };
    if(word == "not")   return token{ token_kind::kw_not, word };
    if(word == "if")    return token{ token_kind::kw_if, word };
    if(word == "else")  return token{ token_kind::kw_else, word };
    if(word == "in")    return token{ token_kind::kw_in, word };
    return token{ token_kind::name, word };
}

token_kind two_char_kind(std::string_view pair)
{
    if(pair == "**") return token_kind::star_star;
    if(pair == "//") return token_kind::slash_slash;
    if(pair == "<=") return token_kind::less_equal;
    if(pair == ">=") return token_kind::greater_equal;
    if(pair == "==") return token_kind::equal_equal;
    if(pair == "!=") return token_kind::not_equal;
    return token_kind::error;
}

token_kind one_char_kind(char c)
{
    switch(c)
    {
        case '+': return token_kind::plus;
        case '-': return token_kind::minus;
        case '*': return token_kind::star;
        case '/': return token_kind::slash;
        case '%': return token_kind::percent;
        case '(': return token_kind::lparen;
        case ')': return token_kind::rparen;
        case '[': return token_kind::lbracket;
        case ']': return token_kind::rbracket;
        case ',': return token_kind::comma;
        case '.': return token_kind::dot;
        case '<': return token_kind::less;
        case '>': return token_kind::greater;
    }
    return token_kind::error;
}

std::optional<value> number_leaf(std::string_view text, bool is_float)
{
    bool ok = false;
    if(!is_float)
    {
        const std::int64_t number = parse_int(text, ok);
        return ok ? std::optional<value>(value{ number }) : std::nullopt;
    }
    const double number = parse_double(text, ok);
    return ok ? value::make_real(number) : std::nullopt;
}

std::size_t push_number(std::string_view src, std::size_t i, std::vector<token> &out)
{
    std::size_t j = i;
    bool is_float = false;
    for(; j < src.size(); ++j)
    {
        char c = src[j];
        bool exp_sign = (c == '+' || c == '-') && (src[j - 1] == 'e' || src[j - 1] == 'E');
        if(c == '.' || c == 'e' || c == 'E') is_float = true;
        else if(!std::isdigit(static_cast<unsigned char>(c)) && !exp_sign) break;
    }
    std::string_view text = src.substr(i, j - i);
    const std::optional<value> leaf = number_leaf(text, is_float);
    out.push_back(token{ leaf ? token_kind::number : token_kind::error, text,
                         leaf.value_or(value{}) });
    return j;
}

// Only the single-quoted spelling is measured upstream, so the double-quoted one and a
// backslash escape refuse as unsupported rather than being given an unmeasured meaning; an
// unterminated literal is a lexical fault and refuses as an error. Every refusal consumes
// the rest of the source, because there is no resynchronization point after one.
std::size_t push_string(std::string_view src, std::size_t i, std::vector<token> &out)
{
    if(src[i] == '"')
    {
        out.push_back(token{ token_kind::unsupported, src.substr(i, 1) });
        return src.size();
    }
    std::size_t j = i + 1;
    while(j < src.size() && src[j] != '\'' && src[j] != '\\') ++j;
    if(j == src.size() || src[j] == '\\')
    {
        const bool unterminated = j == src.size();
        out.push_back(token{ unterminated ? token_kind::error : token_kind::unsupported,
                             src.substr(i) });
        return src.size();
    }
    out.push_back(token{ token_kind::string, src.substr(i, j - i + 1),
                         value{ std::string(src.substr(i + 1, j - i - 1)) } });
    return j + 1;
}

std::size_t push_name(std::string_view src, std::size_t i, std::vector<token> &out)
{
    std::size_t j = i;
    while(j < src.size() && is_name_char(src[j])) ++j;
    out.push_back(classify_name(src.substr(i, j - i)));
    return j;
}

std::size_t push_symbol(std::string_view src, std::size_t i, std::vector<token> &out)
{
    token_kind two = two_char_kind(src.substr(i, 2));
    if(two != token_kind::error) { out.push_back(token{ two, src.substr(i, 2) }); return 2; }
    token_kind one = one_char_kind(src[i]);
    if(one != token_kind::error) { out.push_back(token{ one, src.substr(i, 1) }); return 1; }
    return 0;
}

}

std::vector<token> tokenize(std::string_view source)
{
    std::vector<token> tokens;
    std::size_t i = 0;
    while(i < source.size())
    {
        char c = source[i];
        bool starts_number = std::isdigit(static_cast<unsigned char>(c)) != 0
            || (c == '.' && i + 1 < source.size()
                && std::isdigit(static_cast<unsigned char>(source[i + 1])) != 0);
        if(std::isspace(static_cast<unsigned char>(c)) != 0) { ++i; continue; }
        if(starts_number)              { i = push_number(source, i, tokens); continue; }
        if(is_name_start(c))           { i = push_name(source, i, tokens); continue; }
        if(c == '\'' || c == '"')      { i = push_string(source, i, tokens); continue; }
        std::size_t consumed = push_symbol(source, i, tokens);
        if(consumed == 0) { tokens.push_back(token{ token_kind::unsupported, source.substr(i, 1) }); break; }
        i += consumed;
    }
    tokens.push_back(token{ token_kind::end, {} });
    return tokens;
}

}
