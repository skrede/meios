#include "lexer.h"
#include "eval_parser.h"

#include "meios/xacro/value.h"

#include <string>
#include <vector>
#include <cstddef>
#include <optional>
#include <algorithm>
#include <initializer_list>

namespace meios::detail
{
namespace
{

bool one_of(token_kind kind, std::initializer_list<token_kind> set)
{
    return std::find(set.begin(), set.end(), kind) != set.end();
}

bool starts_value(token_kind kind)
{
    return one_of(kind, { token_kind::number, token_kind::string, token_kind::name,
                          token_kind::kw_true, token_kind::kw_false });
}

// None of these can begin an operand, so meeting one is only well formed once an operand has
// been produced. The argument separator belongs here for the same reason an operator does: it
// closes an argument, and an argument still owing its operand is as malformed as a stream that
// ends owing one.
bool follows_operand(token_kind kind)
{
    return one_of(kind, { token_kind::star, token_kind::slash, token_kind::slash_slash,
                          token_kind::percent, token_kind::star_star, token_kind::less,
                          token_kind::less_equal, token_kind::greater, token_kind::greater_equal,
                          token_kind::equal_equal, token_kind::not_equal, token_kind::kw_and,
                          token_kind::kw_or, token_kind::kw_in, token_kind::kw_if,
                          token_kind::kw_else, token_kind::comma });
}

// A closing bracket completes a value, and so does a token the lexer already rejected -- whose
// own diagnostic is the one the author needs, so the walk must carry on past it rather than
// speak over it.
bool yields_value(token_kind kind)
{
    return starts_value(kind)
        || one_of(kind, { token_kind::rparen, token_kind::rbracket, token_kind::error,
                          token_kind::unsupported });
}

// Alternation of operands and operators, in one left-to-right walk over the whole stream. Only
// the cells that cannot be anything but malformed report here; a bracket, a dot, a lexical
// failure and an empty stream all defer, because the parser reaches each of them with a more
// specific message than this walk could give.
std::optional<std::size_t> first_malformed(const std::vector<token> &tokens)
{
    bool operand_expected = true;
    for(std::size_t i = 0; i < tokens.size(); ++i)
    {
        const token_kind kind = tokens[i].kind;
        if(kind == token_kind::end)
            return operand_expected && i > 0 ? std::optional<std::size_t>(i) : std::nullopt;
        if(operand_expected ? follows_operand(kind) : starts_value(kind)) return i;
        // `not` reads the same in either position: leading an operand, or opening the `not in`
        // spelling the membership parser refuses on its own terms.
        if(kind != token_kind::kw_not) operand_expected = !yields_value(kind);
    }
    return std::nullopt;
}

std::string malformed_message(const token &offender)
{
    if(offender.text.empty())
        return "expression ends where an operand is expected";
    return "unexpected '" + std::string(offender.text) + "' in expression";
}

}

bool refuse_malformed_syntax(parser &p)
{
    const std::optional<std::size_t> offender = first_malformed(p.tokens);
    if(!offender) return false;
    p.fail(malformed_message(p.tokens[*offender]));
    return true;
}

}
