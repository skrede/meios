#ifndef HPP_GUARD_MEIOS_XACRO_EVAL_PARSER_H
#define HPP_GUARD_MEIOS_XACRO_EVAL_PARSER_H

#include "lexer.h"

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"

#include "meios/diagnostic/log_sink.h"

#include <vector>
#include <cstddef>
#include <variant>
#include <string_view>

namespace meios::detail
{

struct parser
{
    parser(const std::vector<token> &token_stream, const eval_scope &names, log_sink &sink)
        : tokens(token_stream), scope(names), log(sink), pos(0), ok(true)
    {}

    const token &peek() const { return tokens[pos]; }
    bool at(token_kind kind) const { return tokens[pos].kind == kind; }
    bool accept(token_kind kind) { if(at(kind)) { ++pos; return true; } return false; }
    value fail(const std::string &message);

    const std::vector<token> &tokens;
    const eval_scope &scope;
    log_sink &log;
    std::size_t pos;
    bool ok;
};

inline bool is_int(const value &v) { return !std::holds_alternative<double>(v); }

inline long long as_int(const value &v)
{
    if(std::holds_alternative<bool>(v)) return std::get<bool>(v) ? 1 : 0;
    if(std::holds_alternative<long long>(v)) return std::get<long long>(v);
    return static_cast<long long>(std::get<double>(v));
}

inline double as_double(const value &v)
{
    if(std::holds_alternative<bool>(v)) return std::get<bool>(v) ? 1.0 : 0.0;
    if(std::holds_alternative<long long>(v)) return static_cast<double>(std::get<long long>(v));
    return std::get<double>(v);
}

inline bool truthy(const value &v)
{
    if(std::holds_alternative<bool>(v)) return std::get<bool>(v);
    if(std::holds_alternative<long long>(v)) return std::get<long long>(v) != 0;
    return std::get<double>(v) != 0.0;
}

value parse_ternary(parser &p);
value parse_or(parser &p);
value parse_and(parser &p);
value parse_not(parser &p);
value parse_comparison(parser &p);
value parse_add(parser &p);
value parse_mul(parser &p);
value parse_unary(parser &p);
value parse_power(parser &p);
value parse_atom(parser &p);
value parse_name(parser &p);
value call_math(parser &p, std::string_view name, const std::vector<value> &args);

}

#endif
