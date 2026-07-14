#include "lexer.h"
#include "eval_parser.h"

#include "meios/xacro/value.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <cmath>
#include <string>
#include <vector>
#include <variant>
#include <cstdlib>

namespace meios::detail
{

value parser::fail(const std::string &message)
{
    if(ok) log.log(level::error, message);
    ok = false;
    return value{ 0ll };
}

namespace
{

value negate(const value &v)
{
    if(std::holds_alternative<double>(v)) return value{ -std::get<double>(v) };
    return value{ -as_int(v) };
}

value unary_pos(const value &v)
{
    if(std::holds_alternative<double>(v)) return v;
    return value{ as_int(v) };
}

value add_sub_mul(const value &a, const value &b, token_kind op)
{
    if(is_int(a) && is_int(b))
    {
        long long x = as_int(a), y = as_int(b);
        if(op == token_kind::plus)  return value{ x + y };
        if(op == token_kind::minus) return value{ x - y };
        return value{ x * y };
    }
    double x = as_double(a), y = as_double(b);
    if(op == token_kind::plus)  return value{ x + y };
    if(op == token_kind::minus) return value{ x - y };
    return value{ x * y };
}

long long floordiv_int(long long a, long long b)
{
    long long q = a / b, r = a % b;
    if(r != 0 && ((r < 0) != (b < 0))) --q;
    return q;
}

long long mod_int(long long a, long long b)
{
    long long r = a % b;
    if(r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}

value divide(parser &p, const value &a, const value &b)
{
    double y = as_double(b);
    if(y == 0.0) return p.fail("division by zero");
    return value{ as_double(a) / y };
}

value floor_divide(parser &p, const value &a, const value &b)
{
    if(is_int(a) && is_int(b))
    {
        if(as_int(b) == 0) return p.fail("integer division by zero");
        return value{ floordiv_int(as_int(a), as_int(b)) };
    }
    double y = as_double(b);
    if(y == 0.0) return p.fail("float floor division by zero");
    return value{ std::floor(as_double(a) / y) };
}

value modulo(parser &p, const value &a, const value &b)
{
    if(is_int(a) && is_int(b))
    {
        if(as_int(b) == 0) return p.fail("integer modulo by zero");
        return value{ mod_int(as_int(a), as_int(b)) };
    }
    double y = as_double(b);
    if(y == 0.0) return p.fail("float modulo by zero");
    double r = std::fmod(as_double(a), y);
    if(r != 0.0 && ((r < 0.0) != (y < 0.0))) r += y;
    return value{ r };
}

value power(const value &a, const value &b)
{
    if(is_int(a) && is_int(b) && as_int(b) >= 0)
    {
        long long base = as_int(a), result = 1;
        for(long long k = 0, exp = as_int(b); k < exp; ++k) result *= base;
        return value{ result };
    }
    return value{ std::pow(as_double(a), as_double(b)) };
}

}

value parse_atom(parser &p)
{
    const token &here = p.peek();
    if(p.accept(token_kind::number))   return here.leaf;
    if(p.accept(token_kind::kw_true))  return value{ true };
    if(p.accept(token_kind::kw_false)) return value{ false };
    if(p.accept(token_kind::lparen))
    {
        value inner = parse_ternary(p);
        if(!p.accept(token_kind::rparen)) return p.fail("expected ')'");
        return inner;
    }
    if(p.at(token_kind::name)) return parse_name(p);
    if(p.at(token_kind::error))
        return p.fail("unrecognized character in expression: '" + std::string(here.text) + "'");
    return p.fail("unsupported expression — use eval-python");
}

value parse_power(parser &p)
{
    value base = parse_atom(p);
    if(p.accept(token_kind::star_star)) return power(base, parse_unary(p));
    return base;
}

value parse_unary(parser &p)
{
    if(p.accept(token_kind::minus)) return negate(parse_unary(p));
    if(p.accept(token_kind::plus))  return unary_pos(parse_unary(p));
    return parse_power(p);
}

value parse_mul(parser &p)
{
    value left = parse_unary(p);
    for(;;)
    {
        token_kind op = p.peek().kind;
        if(op == token_kind::star)              { ++p.pos; left = add_sub_mul(left, parse_unary(p), op); }
        else if(op == token_kind::slash)        { ++p.pos; left = divide(p, left, parse_unary(p)); }
        else if(op == token_kind::slash_slash)  { ++p.pos; left = floor_divide(p, left, parse_unary(p)); }
        else if(op == token_kind::percent)      { ++p.pos; left = modulo(p, left, parse_unary(p)); }
        else break;
    }
    return left;
}

value parse_add(parser &p)
{
    value left = parse_mul(p);
    for(;;)
    {
        token_kind op = p.peek().kind;
        if(op != token_kind::plus && op != token_kind::minus) break;
        ++p.pos;
        left = add_sub_mul(left, parse_mul(p), op);
    }
    return left;
}

}
