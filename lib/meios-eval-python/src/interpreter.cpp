#include "interpreter.h"
#include "kept_interrupt.h"

#include "meios/xacro/detail/wide_integer.h"

#include <pybind11/embed.h>

#include <atomic>
#include <string>
#include <utility>
#include <optional>
#include <exception>

namespace meios::detail
{

namespace
{

struct interpreter_start
{
    std::optional<std::string> start;
    std::optional<std::string> limit;
};

std::optional<std::string> hold_digit_limit()
{
    // The interpreter reads this limit from the environment, so it is held at the evaluator's own ceiling
    // (docs.python.org/3/library/stdtypes.html#integer-string-conversion-length-limitation).
    try
    {
        pybind11::module_::import("sys").attr("set_int_max_str_digits")(wide_digit_ceiling);
        return std::nullopt;
    }
    catch(const std::exception &failed)
    {
        return "the interpreter's integer digit limit could not be held at " + std::to_string(wide_digit_ceiling) + " digits, so a longer result is not refused: " + failed.what();
    }
}

// Imported while SIGINT is kept, so the module's one-time install never lands over the host's disposition later.
void claim_signal_module()
{
    try
    {
        pybind11::module_::import("signal");
    }
    catch(const pybind11::error_already_set &)
    {
    }
}

std::optional<std::string> configured_start()
{
    const kept_interrupt kept;
    try
    {
        PyConfig config;
        PyConfig_InitPythonConfig(&config);
        config.safe_path               = 1;
        config.install_signal_handlers = 0;
        pybind11::initialize_interpreter(&config, 0, nullptr, false);
    }
    catch(const std::exception &failed)
    {
        return "the embedded Python interpreter did not start: " + std::string(failed.what());
    }
    claim_signal_module();
    return std::nullopt;
}

interpreter_start start_interpreter()
{
    // A host that already runs Python owns that interpreter, with its module path, digit limit and
    // signals; the evaluator only takes its lock per call.
    if(Py_IsInitialized() != 0)
        return interpreter_start{};
    // Never finalized: CPython finalize/re-initialize cycles are unreliable for extension modules
    // (e.g. NumPy), so process teardown reclaims the interpreter.
    if(std::optional<std::string> failed = configured_start())
        return interpreter_start{std::move(failed), std::nullopt};
    interpreter_start started{std::nullopt, hold_digit_limit()};
    // Disarmed, the release leaves the lock free for any thread's per-call gil_scoped_acquire.
    pybind11::gil_scoped_release released;
    released.disarm();
    return started;
}

}

std::optional<std::string> interpreter_failure()
{
    static const interpreter_start started = start_interpreter();
    static std::atomic<bool> limit_reported{false};
    if(started.start)
        return started.start;
    if(started.limit && !limit_reported.exchange(true))
        return started.limit;
    return std::nullopt;
}

}
