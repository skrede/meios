#ifndef HPP_GUARD_MEIOS_XACRO_EVAL_NUMERIC_OPS_H
#define HPP_GUARD_MEIOS_XACRO_EVAL_NUMERIC_OPS_H

#include "lexer.h"
#include "eval_parser.h"

#include "meios/xacro/value.h"

#include <string>
#include <cstdint>
#include <optional>
#include <string_view>

namespace meios::detail
{

// Each of these routes an operand with no arithmetic meaning to parser::fail, so the
// precedence levels above them read the same as they did over a three-alternative scalar
// while a string, a sequence or a mapping now fails typed instead of falling through.
std::int64_t need_int(parser &p, const value &v);
double need_double(parser &p, const value &v);
bool need_truth(parser &p, const value &v);
value real_result(parser &p, double number);

// Every string this evaluator produces is built here, so the ceiling on a produced string has one
// charge site rather than one per producer: a producer added later that does not come through
// here is visible as the one that skipped the bound.
value text_result(parser &p, std::string text);

// Answers whether an exponentiation's base magnitude and exponent are within the numeric ceiling,
// asked before the exponentiation runs. The overflow refusal below fires on a result, and a check
// that fires on a result cannot bound the work that produced it.
bool admits_power(parser &p, const value &a, const value &b);

// Each yields no value when the result is not representable, so the operator above it can
// refuse with a located diagnostic instead of wrapping, trapping or aborting the process.
std::optional<std::int64_t> checked_abs(std::int64_t a);
std::optional<std::int64_t> checked_negate(std::int64_t a);
std::optional<std::int64_t> checked_add(std::int64_t a, std::int64_t b);
std::optional<std::int64_t> checked_sub(std::int64_t a, std::int64_t b);
std::optional<std::int64_t> checked_mul(std::int64_t a, std::int64_t b);
std::optional<std::int64_t> checked_mod(std::int64_t a, std::int64_t b);
std::optional<std::int64_t> checked_floordiv(std::int64_t a, std::int64_t b);
std::optional<std::int64_t> checked_power(std::int64_t base, std::int64_t exponent);

value int_from_real(parser &p, double number);
value int_result(parser &p, std::optional<std::int64_t> made, std::string_view operation);

value negate(parser &p, const value &v);
value unary_pos(parser &p, const value &v);
value add_sub_mul(parser &p, const value &a, const value &b, token_kind op);
value divide(parser &p, const value &a, const value &b);
value floor_divide(parser &p, const value &a, const value &b);
value modulo(parser &p, const value &a, const value &b);
value power(parser &p, const value &a, const value &b);

}

#endif
