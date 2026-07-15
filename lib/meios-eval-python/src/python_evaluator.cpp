#include "meios/eval/python_evaluator.h"

#include "interpreter.h"

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/core_evaluator.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <pybind11/embed.h>

#include <cctype>
#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <string_view>

namespace py = pybind11;

namespace meios
{

namespace
{

py::object to_py_object(const binding &bound)
{
    if(std::holds_alternative<std::string>(bound))
        return py::str(std::get<std::string>(bound));
    const value &v = std::get<value>(bound);
    if(std::holds_alternative<bool>(v))
        return py::bool_(std::get<bool>(v));
    if(std::holds_alternative<long long>(v))
        return py::int_(std::get<long long>(v));
    return py::float_(std::get<double>(v));
}

std::vector<std::string> identifiers(std::string_view expr)
{
    std::vector<std::string> names;
    std::size_t i = 0;
    while(i < expr.size())
    {
        if(!std::isalpha(static_cast<unsigned char>(expr[i])) && expr[i] != '_')
        {
            ++i;
            continue;
        }
        std::size_t start = i;
        while(i < expr.size() && (std::isalnum(static_cast<unsigned char>(expr[i])) || expr[i] == '_'))
            ++i;
        names.emplace_back(expr.substr(start, i - start));
    }
    return names;
}

// math is spread into globals (from math import *) so ${sin(pi/2)}-style bare names
// hold parity with the core evaluator; load_yaml lazily imports PyYAML (a found
// environment resource) and a missing module surfaces as a loud runtime error.
void seed_math(py::dict &globals)
{
    py::module_ math = py::module_::import("math");
    globals.attr("update")(math.attr("__dict__"));
    py::exec(
        "def load_yaml(path):\n"
        "    import yaml\n"
        "    with open(path) as _stream:\n"
        "        return yaml.safe_load(_stream)\n",
        globals);
}

void seed_scope(py::dict &globals, std::string_view expr, const eval_scope &scope)
{
    for(const std::string &name : identifiers(expr))
    {
        std::optional<binding> bound = scope.lookup(name);
        if(bound)
            globals[py::str(name)] = to_py_object(*bound);
    }
}

// bool/float route back through the core evaluator's to_python_str so the two
// paths render identically (True/False, .0-append); str returns verbatim. int
// renders via Python's own str() to preserve arbitrary-precision results (e.g.
// ${2**64}) that overflow long long; bool precedes int because in Python bool is
// a subtype of int. Every other object (list, dict) substitutes as its Python str().
std::string format_result(const py::object &result)
{
    if(py::isinstance<py::str>(result))
        return result.cast<std::string>();
    if(py::isinstance<py::bool_>(result))
        return to_python_str(value{ result.cast<bool>() });
    if(py::isinstance<py::float_>(result))
        return to_python_str(value{ result.cast<double>() });
    return py::str(result).cast<std::string>();
}

}

std::optional<std::string> python_evaluator::eval_to_text(std::string_view expr,
                                                          const eval_scope &scope, log_sink &log)
{
    detail::ensure_interpreter();
    py::gil_scoped_acquire gil;
    try
    {
        py::dict globals;
        seed_math(globals);
        seed_scope(globals, expr, scope);
        py::object result = py::eval(std::string(expr), globals);
        m_kind = eval_failure_kind::none;
        return format_result(result);
    }
    catch(py::error_already_set &raised)
    {
        m_kind = eval_failure_kind::error;
        log.log(level::error, "python evaluation of \"" + std::string(expr) + "\" raised: "
            + raised.what());
        return std::nullopt;
    }
    catch(const std::exception &raised)
    {
        m_kind = eval_failure_kind::error;
        log.log(level::error, "python evaluation of \"" + std::string(expr)
            + "\" failed to convert its result: " + raised.what());
        return std::nullopt;
    }
}

}
