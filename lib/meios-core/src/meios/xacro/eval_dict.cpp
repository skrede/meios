#include "lexer.h"
#include "eval_parser.h"

#include "meios/xacro/value.h"

#include "meios/xacro/detail/value_key.h"

#include <string>
#include <vector>
#include <cstddef>
#include <utility>

namespace meios::detail
{
namespace
{

value refuse_shape(parser &p)
{
    return p.fail_unsupported("the mapping constructor takes keyword arguments only, not '"
                              + std::string(p.peek().text) + "' — use eval-python");
}

// A name is only a keyword argument when an assignment follows it; the token after a name is
// always readable, because the stream ends with a token of its own.
bool at_keyword(const parser &p)
{
    return p.at(token_kind::name) && p.tokens[p.pos + 1].kind == token_kind::assign;
}

// Upstream refuses a repeated keyword before it constructs anything, so resolving one here would
// render where upstream raises. The repetition is read off the built mapping's own extent rather
// than searched for, so the first-position last-value rule stays the mapping's alone.
value built_or_repeated(parser &p, std::vector<value::entry> entries)
{
    const std::size_t named = entries.size();
    const value built = value::make_mapping(std::move(entries));
    if(built.size() != named)
        return p.fail("a keyword argument is repeated in the mapping constructor");
    p.exercised(evaluator_construct::mapping_literal);
    return built;
}

}

// The one path an expression has to a mapping of its own, and it leaves what it builds unmarked:
// an authored mapping has no document behind it, so it has no member path either, and every
// other argument shape refuses rather than being given a meaning that was never measured.
value parse_dict(parser &p)
{
    const level_guard level(p);
    if(!level.admitted())
        return value{};
    p.accept(token_kind::lparen);
    std::vector<value::entry> entries;
    if(!p.at(token_kind::rparen))
        do
        {
            if(!at_keyword(p))
                return refuse_shape(p);
            const std::string name(p.peek().text);
            p.pos += 2;
            entries.emplace_back(scalar_key(name), parse_ternary(p));
            if(!p.ok)
                return value{};
        } while(p.accept(token_kind::comma));
    if(!p.accept(token_kind::rparen))
        return p.fail("expected ')' after arguments");
    return built_or_repeated(p, std::move(entries));
}

}
