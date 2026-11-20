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

class big_integer;

// need_int and need_double route an operand with no arithmetic meaning (a string, sequence or mapping) to parser::fail, need_double
// an integer too large for a real too; it refuses as exhausted when the step ceiling refuses the conversion. After any refusal it
// answers 0.0 and the parser keeps the first failure, so a caller's following fail does nothing. need_truth answers any value's truth.
std::int64_t need_int(parser &p, const value &v);
double need_double(parser &p, const value &v);
bool need_truth(parser &p, const value &v);
value real_result(parser &p, double number);

// Every string this evaluator produces is built here, so the ceiling on a produced string has one
// charge site rather than one per producer: a producer added later that does not come through
// here is visible as the one that skipped the bound.
value text_result(parser &p, std::string text);

// Answers whether one more element may be built into a collection the evaluator is producing,
// asked before that element is built. The byte and string-length axes charge an element's
// contents, so an element with no contents costs nothing on either; this is what bounds how many
// of them one expression may produce.
bool admits_element(parser &p);

// Answers whether the real operands of a power computed as a real are within the numeric ceiling,
// asked before the exponentiation runs, since a check on its result cannot bound the work.
bool admits_power(parser &p, const value &a, const value &b);

// Converting a wide integer's digits costs work that grows faster than their count; the 4300-digit cap
// is what lets one evaluation step per nine digits bound it, while a result that fits 64 bits is
// charged nothing else.
bool admits_digits(parser &p, const value &v);

// Each yields no value when the result is not representable in 64 bits, so the operator above it
// computes the exact integer or refuses with a located diagnostic instead of wrapping, trapping or
// aborting the process.
std::optional<std::int64_t> checked_abs(std::int64_t a);
std::optional<std::int64_t> checked_negate(std::int64_t a);
std::optional<std::int64_t> checked_add(std::int64_t a, std::int64_t b);
std::optional<std::int64_t> checked_sub(std::int64_t a, std::int64_t b);
std::optional<std::int64_t> checked_mul(std::int64_t a, std::int64_t b);
std::optional<std::int64_t> checked_mod(std::int64_t a, std::int64_t b);
std::optional<std::int64_t> checked_floordiv(std::int64_t a, std::int64_t b);
std::optional<std::int64_t> checked_power(std::int64_t base, std::int64_t exponent);

// A boolean, an integer of 64 bits and a wide integer are integral; a boolean reads as 0 or 1.
bool is_integral(const value &v);
big_integer big_integer_of(const value &v);

// Every integer result passes here: an integer where it fits 64 bits, otherwise a wide integer
// charged its digits as a produced string is charged, and past the digit ceiling a crossed ceiling.
value integer_result(parser &p, const big_integer &number, std::string_view operation);

value integer_arithmetic(parser &p, const value &a, const value &b, token_kind op);
value integer_negate(parser &p, const value &v);
value integer_abs(parser &p, const value &v);

// An integral base and a non-negative integral exponent. Such a power is bounded by the digit
// ceiling, its result's length estimated before any multiplication, and not by the numeric ceiling.
bool integral_power(const value &base, const value &exponent);
value integer_power(parser &p, const value &a, const value &b);

// A zero divisor refuses naming its operator; '/' answers the quotient rounded once.
value integer_floor_divide(parser &p, const value &a, const value &b);
value integer_modulo(parser &p, const value &a, const value &b);
value integer_divide(parser &p, const value &a, const value &b);

value negate(parser &p, const value &v);
value unary_pos(parser &p, const value &v);
value add_sub_mul(parser &p, const value &a, const value &b, token_kind op);
value divide(parser &p, const value &a, const value &b);
value floor_divide(parser &p, const value &a, const value &b);
value modulo(parser &p, const value &a, const value &b);
value power(parser &p, const value &a, const value &b);

}

#endif
