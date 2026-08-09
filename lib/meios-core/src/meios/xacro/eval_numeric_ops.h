#ifndef HPP_GUARD_MEIOS_XACRO_EVAL_NUMERIC_OPS_H
#define HPP_GUARD_MEIOS_XACRO_EVAL_NUMERIC_OPS_H

#include "lexer.h"
#include "eval_parser.h"

#include "meios/xacro/value.h"

#include <cstdint>

namespace meios::detail
{

// Each of these routes an operand with no arithmetic meaning to parser::fail, so the
// precedence levels above them read the same as they did over a three-alternative scalar
// while a string, a sequence or a mapping now fails typed instead of falling through.
std::int64_t need_int(parser &p, const value &v);
double need_double(parser &p, const value &v);
bool need_truth(parser &p, const value &v);
value real_result(parser &p, double number);

value negate(parser &p, const value &v);
value unary_pos(parser &p, const value &v);
value add_sub_mul(parser &p, const value &a, const value &b, token_kind op);
value divide(parser &p, const value &a, const value &b);
value floor_divide(parser &p, const value &a, const value &b);
value modulo(parser &p, const value &a, const value &b);
value power(parser &p, const value &a, const value &b);

}

#endif
