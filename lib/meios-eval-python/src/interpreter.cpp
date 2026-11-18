#include "interpreter.h"

#include "meios/xacro/detail/wide_integer.h"

#include <pybind11/embed.h>

namespace meios::detail
{

namespace
{

pybind11::scoped_interpreter &started_interpreter()
{
    pybind11::scoped_interpreter &started = *new pybind11::scoped_interpreter();
    // The interpreter reads this limit from the environment, so it is held at the evaluator's own ceiling
    // (docs.python.org/3/library/stdtypes.html#integer-string-conversion-length-limitation).
    const pybind11::object hold = pybind11::getattr(pybind11::module_::import("sys"), "set_int_max_str_digits", pybind11::none());
    if(!hold.is_none())
        hold(wide_digit_ceiling);
    return started;
}

}

void ensure_interpreter()
{
    // Leaked deliberately: the embedded interpreter is initialized once on first use
    // and never finalized -- CPython finalize/re-initialize cycles are unreliable for
    // extension modules (e.g. NumPy). Process teardown reclaims it.
    static pybind11::scoped_interpreter &guard = started_interpreter();
    (void)guard;
    // scoped_interpreter leaves the GIL held by the initializing thread; release it
    // once (also leaked) so any thread's per-call gil_scoped_acquire can take it.
    static pybind11::gil_scoped_release &released = *new pybind11::gil_scoped_release();
    (void)released;
}

}
