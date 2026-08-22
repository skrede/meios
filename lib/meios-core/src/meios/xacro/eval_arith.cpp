#include "lexer.h"
#include "eval_parser.h"
#include "eval_numeric_ops.h"

#include "meios/xacro/value.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <string>
#include <string_view>

namespace meios::detail
{

value parser::fail(const std::string &message, diagnostic_code code)
{
    if(ok) { log.log(level::error, code, anchor, message); failure = eval_failure_kind::error; }
    ok = false;
    return value{};
}

value parser::fail_unsupported(const std::string &message)
{
    if(ok)
    {
        log.log(level::error, diagnostic_code::unsupported_expression, anchor, message);
        failure = eval_failure_kind::unsupported;
    }
    ok = false;
    return value{};
}

bool parser::refuse_exhausted()
{
    if(ok) failure = eval_failure_kind::exhausted;
    ok = false;
    return false;
}

bool parser::enter_level()
{
    if(!session.charge_steps(1, log, anchor)) return refuse_exhausted();
    if(!session.admits_expression_depth(nesting + 1, log, anchor)) return refuse_exhausted();
    ++nesting;
    return true;
}

namespace
{

std::string lexical_message(std::string_view text)
{
    if(!text.empty() && text.front() == '\'')
        return "unterminated string literal in expression: " + std::string(text);
    return "unrecognized character in expression: '" + std::string(text) + "'";
}

// The lexeme the walk stopped on is what a description author needs; only the end of the
// stream has none to name.
std::string unsupported_message(std::string_view text)
{
    if(text.empty())
        return "unsupported expression — use eval-python";
    return "unsupported expression at '" + std::string(text) + "' — use eval-python";
}

}

value parse_atom(parser &p)
{
    const level_guard level(p);
    if(!level.admitted()) return value{};
    const token &here = p.peek();
    if(p.accept(token_kind::number))   return here.leaf;
    if(p.accept(token_kind::string))   return here.leaf;
    if(p.accept(token_kind::kw_true))  return value{ true };
    if(p.accept(token_kind::kw_false)) return value{ false };
    if(p.accept(token_kind::lparen))
    {
        value inner = parse_ternary(p);
        if(!p.accept(token_kind::rparen)) return p.fail("expected ')'");
        return inner;
    }
    if(p.at(token_kind::name)) return parse_name(p);
    if(p.at(token_kind::error)) return p.fail(lexical_message(here.text));
    return p.fail_unsupported(unsupported_message(here.text));
}

value parse_power(parser &p)
{
    const level_guard level(p);
    if(!level.admitted()) return value{};
    value base = parse_postfix(p);
    if(p.accept(token_kind::star_star)) return power(p, base, parse_unary(p));
    return base;
}

value parse_unary(parser &p)
{
    const level_guard level(p);
    if(!level.admitted()) return value{};
    if(p.accept(token_kind::minus)) return negate(p, parse_unary(p));
    if(p.accept(token_kind::plus))  return unary_pos(p, parse_unary(p));
    return parse_power(p);
}

value parse_mul(parser &p)
{
    const level_guard level(p);
    if(!level.admitted()) return value{};
    value left = parse_unary(p);
    for(;;)
    {
        token_kind op = p.peek().kind;
        if(op == token_kind::star)              { ++p.pos; left = add_sub_mul(p, left, parse_unary(p), op); }
        else if(op == token_kind::slash)        { ++p.pos; left = divide(p, left, parse_unary(p)); }
        else if(op == token_kind::slash_slash)  { ++p.pos; left = floor_divide(p, left, parse_unary(p)); }
        else if(op == token_kind::percent)      { ++p.pos; left = modulo(p, left, parse_unary(p)); }
        else break;
    }
    return left;
}

value parse_add(parser &p)
{
    const level_guard level(p);
    if(!level.admitted()) return value{};
    value left = parse_mul(p);
    for(;;)
    {
        token_kind op = p.peek().kind;
        if(op != token_kind::plus && op != token_kind::minus) break;
        ++p.pos;
        left = add_sub_mul(p, left, parse_mul(p), op);
    }
    return left;
}

}
