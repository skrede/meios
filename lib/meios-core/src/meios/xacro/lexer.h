#ifndef HPP_GUARD_MEIOS_XACRO_LEXER_H
#define HPP_GUARD_MEIOS_XACRO_LEXER_H

#include "meios/xacro/value.h"

#include <string>
#include <vector>
#include <string_view>

namespace meios::detail
{

enum class token_kind
{
    number,
    string,
    name,
    kw_true,
    kw_false,
    kw_and,
    kw_or,
    kw_not,
    kw_if,
    kw_else,
    kw_in,
    plus,
    minus,
    star,
    slash,
    slash_slash,
    percent,
    star_star,
    lparen,
    rparen,
    lbracket,
    rbracket,
    comma,
    dot,
    less,
    less_equal,
    greater,
    greater_equal,
    equal_equal,
    not_equal,
    assign,
    end,
    error,
    unsupported,
};

struct token
{
    token_kind kind;
    std::string_view text;
    value leaf;
    std::string spelling;
};

// A name written outside ASCII is read under the NFKC spelling its token carries.
inline std::string_view name_of(const token &one)
{
    return one.spelling.empty() ? one.text : std::string_view(one.spelling);
}

std::vector<token> tokenize(std::string_view source);

}

#endif
