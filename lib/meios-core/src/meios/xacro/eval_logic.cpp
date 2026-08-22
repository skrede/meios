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
    return int_result(p, checked_abs(need_int(p, v)), "absolute value");
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
    if(name == "floor")   return int_from_real(p, std::floor(need_double(p, arg)));
    if(name == "ceil")    return int_from_real(p, std::ceil(need_double(p, arg)));
    if(name == "radians") return real_result(p, need_double(p, arg) * (3.141592653589793 / 180.0));
    if(name == "degrees") return real_result(p, need_double(p, arg) * (180.0 / 3.141592653589793));
    math_fn fn = unary_math_fn(name);
    if(fn != nullptr) return real_result(p, fn(need_double(p, arg)));
    return p.fail_unsupported("unsupported function '" + std::string(name) + "' — use eval-python");
}

bool is_constructor(std::string_view name)
{
    return name == "list" || name == "set" || name == "tuple";
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
    const level_guard level(p);
    if(!level.admitted()) return value{};
    std::string_view name = p.peek().text;
    ++p.pos;
    if(p.accept(token_kind::dot)) return parse_dotted(p, name);
    if(p.at(token_kind::lparen) && name == "dict") return parse_dict(p);
    if(p.at(token_kind::lparen) && is_constructor(name))
        return p.fail_unsupported("the Python constructor '" + std::string(name)
                                  + "()' — use eval-python");
    if(p.at(token_kind::lparen)) return parse_call(p, name);
    if(name == "pi") return real_result(p, 3.141592653589793);
    std::optional<value> bound = p.scope.lookup(name);
    if(!bound)
        return p.fail("name '" + std::string(name) + "' is not defined",
                      diagnostic_code::undefined_property);
    return *bound;
}

value call_math(parser &p, std::string_view name, const std::vector<value> &args)
{
    if(name == "min" || name == "max") return math_reduce(p, name, args);
    if(name == "atan2" && args.size() == 2)
        return real_result(p, std::atan2(need_double(p, args[0]), need_double(p, args[1])));
    if(args.size() == 1) return call_unary(p, name, args[0]);
    return p.fail_unsupported("unsupported function '" + std::string(name) + "' — use eval-python");
}

value parse_not(parser &p)
{
    const level_guard level(p);
    if(!level.admitted()) return value{};
    if(p.accept(token_kind::kw_not)) return value{ !need_truth(p, parse_not(p)) };
    return parse_membership(p);
}

}
