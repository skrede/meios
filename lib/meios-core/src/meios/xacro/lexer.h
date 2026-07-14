#ifndef HPP_GUARD_MEIOS_XACRO_LEXER_H
#define HPP_GUARD_MEIOS_XACRO_LEXER_H

#include "meios/xacro/value.h"

#include <vector>
#include <string_view>

namespace meios::detail
{

enum class token_kind
{
    number,
    name,
    kw_true,
    kw_false,
    kw_and,
    kw_or,
    kw_not,
    kw_if,
    kw_else,
    plus,
    minus,
    star,
    slash,
    slash_slash,
    percent,
    star_star,
    lparen,
    rparen,
    comma,
    less,
    less_equal,
    greater,
    greater_equal,
    equal_equal,
    not_equal,
    end,
    error,
};

struct token
{
    token_kind kind;
    std::string_view text;
    value leaf;
};

std::vector<token> tokenize(std::string_view source);

}

#endif
