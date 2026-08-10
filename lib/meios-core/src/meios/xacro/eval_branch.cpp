#include "lexer.h"
#include "eval_parser.h"
#include "eval_numeric_ops.h"

#include "meios/xacro/value.h"

#include <cstddef>

namespace meios::detail
{
namespace
{

// The evaluator evaluates as it parses and cannot un-evaluate, so taking the first branch means
// rewinding to where the conditional was entered and then jumping to the end its group already
// records — never re-reading the branch that was not taken.
value first_branch(parser &p, std::size_t entry, std::size_t after)
{
    p.pos = entry;
    const value taken = parse_or(p);
    p.pos = after;
    return taken;
}

}

value parse_and(parser &p)
{
    if(!p.charge_step()) return value{};
    value left = parse_not(p);
    while(p.accept(token_kind::kw_and))
    {
        const bool decided = !need_truth(p, left);
        if(decided) p.pos = p.spans.and_operand_end(p.pos);
        left = decided ? value{ false } : value{ need_truth(p, parse_not(p)) };
    }
    return left;
}

value parse_or(parser &p)
{
    if(!p.charge_step()) return value{};
    value left = parse_and(p);
    while(p.accept(token_kind::kw_or))
    {
        const bool decided = need_truth(p, left);
        if(decided) p.pos = p.spans.or_operand_end(p.pos);
        left = decided ? value{ true } : value{ need_truth(p, parse_and(p)) };
    }
    return left;
}

value parse_ternary(parser &p)
{
    if(!p.charge_step()) return value{};
    const std::size_t entry = p.pos;
    const std::size_t conditional = p.spans.conditional_at(entry);
    if(conditional == entry) return parse_or(p);
    const std::size_t after = p.spans.group_end(entry);
    p.pos = conditional + 1;
    const value condition = parse_or(p);
    if(!p.accept(token_kind::kw_else)) return p.fail("expected 'else' in conditional");
    if(!p.ok) return value{};
    const bool taken = need_truth(p, condition);
    if(!p.ok) return value{};
    return taken ? first_branch(p, entry, after) : parse_ternary(p);
}

}
