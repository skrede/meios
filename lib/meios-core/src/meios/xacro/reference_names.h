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

// The names upstream's table of built-ins holds beneath every property, as its create_global_symbols
// builds them under the pinned interpreter; tests/golden/oracle/global_symbols.cases records them.
inline constexpr std::string_view root_symbols[] = {"False",  "True",      "abs_filename", "acos", "acosh",     "asin",     "asinh",  "atan",    "atan2",  "atanh",     "bool",  "cbrt",
                                                    "ceil",   "comb",      "copysign",     "cos",  "cosh",      "degrees",  "dict",   "dist",    "dotify", "e",         "erf",   "erfc",
                                                    "exp",    "exp2",      "expm1",        "fabs", "factorial", "float",    "floor",  "fma",     "fmod",   "frexp",     "fsum",  "gamma",
                                                    "gcd",    "hypot",     "inf",          "int",  "isclose",   "isfinite", "isinf",  "isnan",   "isqrt",  "lcm",       "ldexp", "len",
                                                    "lgamma", "list",      "load_yaml",    "log",  "log10",     "log1p",    "log2",   "map",     "math",   "max",       "min",   "modf",
                                                    "nan",    "nextafter", "perm",         "pi",   "pow",       "prod",     "python", "radians", "range",  "remainder", "round", "sin",
                                                    "sinh",   "sorted",    "sqrt",         "str",  "sumprod",   "tan",      "tanh",   "tau",     "trunc",  "ulp",       "xacro"};

// The builtins the python evaluator's guard prelude binds bare.
inline constexpr std::string_view python_builtins[] = {"abs",  "min",   "max", "round", "len",    "str",   "int",       "float", "bool", "list",
                                                       "dict", "tuple", "set", "sum",   "sorted", "range", "enumerate", "zip",   "any",  "all"};

inline bool listed(std::span<const std::string_view> names, std::string_view name)
{
    return std::ranges::find(names, name) != names.end();
}

}

#endif
