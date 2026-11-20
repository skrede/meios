#include "interpreter.h"

#include "meios/xacro/detail/wide_integer.h"

#include <pybind11/embed.h>

namespace meios::detail
{

namespace
{

void hold_digit_limit()
{
    // The interpreter reads this limit from the environment, so it is held at the evaluator's own ceiling
    // (docs.python.org/3/library/stdtypes.html#integer-string-conversion-length-limitation).
    try
    {
        const pybind11::object hold = pybind11::getattr(pybind11::module_::import("sys"), "set_int_max_str_digits", pybind11::none());
        if(!hold.is_none())
            hold(wide_digit_ceiling);
    }
    // A limit that cannot be held leaves the environment's, and the lock must still be released.
    catch(const pybind11::error_already_set &)
    {
    }
}

bool start_interpreter()
{
    // Never finalized: CPython finalize/re-initialize cycles are unreliable for extension modules
    // (e.g. NumPy), so process teardown reclaims the interpreter.
    pybind11::initialize_interpreter();
    hold_digit_limit();
    // Disarmed, the release leaves the lock free for any thread's per-call gil_scoped_acquire.
    pybind11::gil_scoped_release released;
    released.disarm();
    return true;
}

}

void ensure_interpreter()
{
    static const bool started = start_interpreter();
    (void)started;
}

}
