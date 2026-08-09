#include "lexer.h"
#include "eval_parser.h"
#include "eval_numeric_ops.h"

#include "meios/xacro/value.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <cmath>
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

using math_fn = double (*)(double);

math_fn unary_math_fn(std::string_view name)
{
    if(name == "sin")  return static_cast<math_fn>(std::sin);
    if(name == "cos")  return static_cast<math_fn>(std::cos);
    if(name == "tan")  return static_cast<math_fn>(std::tan);
    if(name == "asin") return static_cast<math_fn>(std::asin);
    if(name == "acos") return static_cast<math_fn>(std::acos);
    if(name == "atan") return static_cast<math_fn>(std::atan);
    if(name == "sqrt") return static_cast<math_fn>(std::sqrt);
    return nullptr;
}

value math_abs(parser &p, const value &v)
{
    if(v.kind() == value_kind::real)
        return real_result(p, std::fabs(*v.real()));
    const std::int64_t number = need_int(p, v);
    return value{ number < 0 ? -number : number };
}

value math_reduce(parser &p, std::string_view name, const std::vector<value> &args)
{
    if(args.empty()) return p.fail(std::string(name) + "() needs an argument");
    value best = args[0];
    bool want_max = name == "max";
    for(std::size_t i = 1; i < args.size(); ++i)
        if((need_double(p, args[i]) > need_double(p, best)) == want_max) best = args[i];
    return best;
}

value call_unary(parser &p, std::string_view name, const value &arg)
{
    if(name == "abs")     return math_abs(p, arg);
    if(name == "floor")   return value{ static_cast<std::int64_t>(std::floor(need_double(p, arg))) };
    if(name == "ceil")    return value{ static_cast<std::int64_t>(std::ceil(need_double(p, arg))) };
    if(name == "radians") return real_result(p, need_double(p, arg) * (3.141592653589793 / 180.0));
    if(name == "degrees") return real_result(p, need_double(p, arg) * (180.0 / 3.141592653589793));
    math_fn fn = unary_math_fn(name);
    if(fn != nullptr) return real_result(p, fn(need_double(p, arg)));
    return p.fail_unsupported("unsupported function '" + std::string(name) + "' — use eval-python");
}

bool is_comparison(token_kind k)
{
    return k == token_kind::less || k == token_kind::less_equal || k == token_kind::greater
        || k == token_kind::greater_equal || k == token_kind::equal_equal || k == token_kind::not_equal;
}

bool compare_pair(parser &p, token_kind op, const value &a, const value &b)
{
    double x = need_double(p, a), y = need_double(p, b);
    switch(op)
    {
        case token_kind::less:          return x <  y;
        case token_kind::less_equal:    return x <= y;
        case token_kind::greater:       return x >  y;
        case token_kind::greater_equal: return x >= y;
        case token_kind::equal_equal:   return x == y;
        case token_kind::not_equal:     return x != y;
        default:                        return false;
    }
}

value parse_call(parser &p, std::string_view name)
{
    p.accept(token_kind::lparen);
    std::vector<value> args;
    if(!p.at(token_kind::rparen))
        do { args.push_back(parse_ternary(p)); } while(p.accept(token_kind::comma));
    if(!p.accept(token_kind::rparen)) return p.fail("expected ')' after arguments");
    return call_math(p, name, args);
}

}

value parse_name(parser &p)
{
    std::string_view name = p.peek().text;
    ++p.pos;
    if(p.at(token_kind::lparen)) return parse_call(p, name);
    if(name == "pi") return real_result(p, 3.141592653589793);
    std::optional<value> bound = p.scope.lookup(name);
    if(!bound)
        return p.fail("name '" + std::string(name) + "' is not defined",
                      diagnostic_code::undefined_property);
    if(bound->kind() != value_kind::string)
        return *bound;
    return p.fail_unsupported("string value '" + std::string(name) + "' in expression — use eval-python");
}

value call_math(parser &p, std::string_view name, const std::vector<value> &args)
{
    if(name == "min" || name == "max") return math_reduce(p, name, args);
    if(name == "atan2" && args.size() == 2)
        return real_result(p, std::atan2(need_double(p, args[0]), need_double(p, args[1])));
    if(args.size() == 1) return call_unary(p, name, args[0]);
    return p.fail_unsupported("unsupported function '" + std::string(name) + "' — use eval-python");
}

value parse_comparison(parser &p)
{
    value left = parse_add(p);
    if(!is_comparison(p.peek().kind)) return left;
    bool result = true;
    while(is_comparison(p.peek().kind))
    {
        token_kind op = p.peek().kind;
        ++p.pos;
        value right = parse_add(p);
        result = result && compare_pair(p, op, left, right);
        left = right;
    }
    return value{ result };
}

value parse_not(parser &p)
{
    if(p.accept(token_kind::kw_not)) return value{ !need_truth(p, parse_not(p)) };
    return parse_comparison(p);
}

value parse_and(parser &p)
{
    value left = parse_not(p);
    while(p.accept(token_kind::kw_and))
    {
        value right = parse_not(p);
        left = value{ need_truth(p, left) && need_truth(p, right) };
    }
    return left;
}

value parse_or(parser &p)
{
    value left = parse_and(p);
    while(p.accept(token_kind::kw_or))
    {
        value right = parse_and(p);
        left = value{ need_truth(p, left) || need_truth(p, right) };
    }
    return left;
}

value parse_ternary(parser &p)
{
    value first = parse_or(p);
    if(!p.accept(token_kind::kw_if)) return first;
    value condition = parse_or(p);
    if(!p.accept(token_kind::kw_else)) return p.fail("expected 'else' in conditional");
    value second = parse_ternary(p);
    return need_truth(p, condition) ? first : second;
}

}
