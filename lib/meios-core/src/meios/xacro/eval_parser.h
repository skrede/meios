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

#include <span>
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
          session(load), anchor(origin), pos(0), nesting(0), ok(true),
          failure(eval_failure_kind::none)
    {}

    const token &peek() const { return tokens[pos]; }
    bool at(token_kind kind) const { return tokens[pos].kind == kind; }
    bool accept(token_kind kind) { if(at(kind)) { ++pos; return true; } return false; }
    value fail(const std::string &message, diagnostic_code code = diagnostic_code::expression_error);
    value fail_unsupported(const std::string &message);
    // Charges the two axes a recursive descent into the grammar grows, before it recurses:
    // one evaluation step, and one more level of nesting. The session reports and latches a
    // crossed ceiling itself, so a refusal only records the category here.
    bool enter_level();
    void leave_level() { --nesting; }
    bool refuse_exhausted();
    // Marked where the construct is exercised and nowhere else, so what a load reports is
    // what it did rather than what a reader believed it would do.
    void exercised(evaluator_construct one) { session.counters.constructs.mark(one); }

    const std::vector<token> &tokens;
    token_spans spans;
    const eval_scope &scope;
    log_sink &log;
    eval_session &session;
    source_location anchor;
    std::size_t pos;
    std::size_t nesting;
    bool ok;
    eval_failure_kind failure;
};

// Every level of the descent enters through one of these, so the level a refused entry
// abandons is released by the same unwind that abandons it. A manual decrement would have
// to be repeated at each of a level's several early returns, and the one that was forgotten
// is the leak.
struct level_guard
{
    explicit level_guard(parser &owner) : p(owner), entered(owner.enter_level()) {}

    level_guard(const level_guard &) = delete;

    ~level_guard() { if(entered) p.leave_level(); }

    bool admitted() const { return entered; }

private:
    parser &p;
    bool entered;
};

inline bool is_int(const value &v)
{
    return v.kind() == value_kind::boolean || v.kind() == value_kind::integer;
}

// The largest representable integer is not itself representable as a double -- it rounds up
// to 2^63 -- so comparing against it would admit a value one step too large. The bound is
// the smallest excluded power of two, compared strictly; a not-a-number fails both
// comparisons and is excluded with everything else out of range.
inline std::optional<std::int64_t> int_from_double(double number)
{
    constexpr double excluded = 9223372036854775808.0;
    if(!(number >= -excluded && number < excluded))
        return std::nullopt;
    return static_cast<std::int64_t>(number);
}

inline std::optional<std::int64_t> as_int(const value &v)
{
    if(std::optional<bool> flag = v.boolean())
        return *flag ? 1 : 0;
    if(std::optional<std::int64_t> number = v.integer())
        return number;
    if(std::optional<double> number = v.real())
        return int_from_double(*number);
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

// Null and a string are the non-arithmetic kinds with a settled truth value here: null is
// false, and a string is false exactly when it holds no bytes. A collection has one in Python
// too, by the same emptiness rule, but that meaning is outside the measured surface and is
// refused rather than approximated.
inline std::optional<bool> truthy(const value &v)
{
    if(v.kind() == value_kind::null)
        return false;
    if(v.kind() == value_kind::string)
        return !v.text()->empty();
    if(std::optional<double> number = as_double(v))
        return *number != 0.0;
    return std::nullopt;
}

bool refuse_malformed_syntax(parser &p);
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
value parse_dict(parser &p);
value parse_dotted(parser &p, std::string_view head);
value parse_load_yaml(parser &p);
value parse_member(parser &p, const value &base);
value member_of(parser &p, const value &base, std::string_view member);
value index_into(parser &p, const value &container, const value &key);
std::span<const std::string_view> colliding_members();
value call_math(parser &p, std::string_view name, const std::vector<value> &args);

}

#endif
