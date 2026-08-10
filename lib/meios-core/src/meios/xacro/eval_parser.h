#ifndef HPP_GUARD_MEIOS_XACRO_EVAL_PARSER_H
#define HPP_GUARD_MEIOS_XACRO_EVAL_PARSER_H

#include "lexer.h"
#include "eval_spans.h"
#include "eval_session.h"

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/core_evaluator.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"

#include <vector>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <string_view>

namespace meios::detail
{

struct parser
{
    parser(const std::vector<token> &token_stream, const eval_scope &names, log_sink &sink,
           eval_session &load, const source_location &origin = {})
        : tokens(token_stream), spans(build_token_spans(token_stream)), scope(names), log(sink),
          session(load), anchor(origin), pos(0), ok(true), failure(eval_failure_kind::none)
    {}

    const token &peek() const { return tokens[pos]; }
    bool at(token_kind kind) const { return tokens[pos].kind == kind; }
    bool accept(token_kind kind) { if(at(kind)) { ++pos; return true; } return false; }
    value fail(const std::string &message, diagnostic_code code = diagnostic_code::expression_error);
    value fail_unsupported(const std::string &message);
    // Charged at the entry of every precedence level, before it recurses. The session
    // reports and latches a crossed ceiling itself, so a refused charge only records the
    // category here and the caller returns immediately with the poison value.
    bool charge_step();

    const std::vector<token> &tokens;
    token_spans spans;
    const eval_scope &scope;
    log_sink &log;
    eval_session &session;
    source_location anchor;
    std::size_t pos;
    bool ok;
    eval_failure_kind failure;
};

inline bool is_int(const value &v)
{
    return v.kind() == value_kind::boolean || v.kind() == value_kind::integer;
}

inline std::optional<std::int64_t> as_int(const value &v)
{
    if(std::optional<bool> flag = v.boolean())
        return *flag ? 1 : 0;
    if(std::optional<std::int64_t> number = v.integer())
        return number;
    if(std::optional<double> number = v.real())
        return static_cast<std::int64_t>(*number);
    return std::nullopt;
}

inline std::optional<double> as_double(const value &v)
{
    if(std::optional<bool> flag = v.boolean())
        return *flag ? 1.0 : 0.0;
    if(std::optional<std::int64_t> number = v.integer())
        return static_cast<double>(*number);
    return v.real();
}

// Null is the one non-arithmetic kind with a settled truth value; a string or a
// collection has one in Python but not in the subset this evaluator claims, so it is
// refused rather than approximated.
inline std::optional<bool> truthy(const value &v)
{
    if(v.kind() == value_kind::null)
        return false;
    if(std::optional<double> number = as_double(v))
        return *number != 0.0;
    return std::nullopt;
}

value parse_ternary(parser &p);
value parse_or(parser &p);
value parse_and(parser &p);
value parse_not(parser &p);
value parse_membership(parser &p);
value parse_comparison(parser &p);
value parse_add(parser &p);
value parse_mul(parser &p);
value parse_unary(parser &p);
value parse_power(parser &p);
value parse_postfix(parser &p);
value parse_atom(parser &p);
value parse_name(parser &p);
value parse_dotted(parser &p, std::string_view head);
value call_math(parser &p, std::string_view name, const std::vector<value> &args);

}

#endif
