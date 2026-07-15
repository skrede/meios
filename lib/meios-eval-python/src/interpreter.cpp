#include "interpreter.h"

#include <pybind11/embed.h>

namespace meios::detail
{

void ensure_interpreter()
{
    // Leaked deliberately: the embedded interpreter is initialized once on first use
    // and never finalized -- CPython finalize/re-initialize cycles are unreliable for
    // extension modules (e.g. NumPy). Process teardown reclaims it.
    static pybind11::scoped_interpreter &guard = *new pybind11::scoped_interpreter();
    (void)guard;
    // scoped_interpreter leaves the GIL held by the initializing thread; release it
    // once (also leaked) so any thread's per-call gil_scoped_acquire can take it.
    static pybind11::gil_scoped_release &released = *new pybind11::gil_scoped_release();
    (void)released;
}

}
