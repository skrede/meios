#ifndef HPP_GUARD_MEIOS_XACRO_REFERENCE_NAMES_H
#define HPP_GUARD_MEIOS_XACRO_REFERENCE_NAMES_H

#include <span>
#include <algorithm>
#include <string_view>

namespace meios::detail
{

// Every public name of Python's mathematics module, taken from Python 3.14. Upstream's evaluation
// command resolves each before an argument, and the python evaluator binds each bare.
inline constexpr std::string_view mathematics_names[] = {
        "acos",    "acosh",    "asin",  "asinh",   "atan",      "atan2", "atanh",     "cbrt",   "ceil",    "comb",  "copysign", "cos",  "cosh",  "degrees", "dist",      "e",
        "erf",     "erfc",     "exp",   "exp2",    "expm1",     "fabs",  "factorial", "floor",  "fma",     "fmod",  "frexp",    "fsum", "gamma", "gcd",     "hypot",     "inf",
        "isclose", "isfinite", "isinf", "isnan",   "isqrt",     "lcm",   "ldexp",     "lgamma", "log",     "log10", "log1p",    "log2", "modf",  "nan",     "nextafter", "perm",
        "pi",      "pow",      "prod",  "radians", "remainder", "sin",   "sinh",      "sqrt",   "sumprod", "tan",   "tanh",     "tau",  "trunc", "ulp"};

// The builtins the python evaluator's guard prelude binds bare.
inline constexpr std::string_view python_builtins[] = {"abs",  "min",   "max", "round", "len",    "str",   "int",       "float", "bool", "list",
                                                       "dict", "tuple", "set", "sum",   "sorted", "range", "enumerate", "zip",   "any",  "all"};

inline bool listed(std::span<const std::string_view> names, std::string_view name)
{
    return std::ranges::find(names, name) != names.end();
}

}

#endif
