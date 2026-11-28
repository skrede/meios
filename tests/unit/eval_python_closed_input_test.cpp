#include "eval_python_fixture.h"

#include "kept_stream_modes.h"

#include <pybind11/embed.h>

#include <catch2/catch_test_macros.hpp>

#if defined(_WIN32)
    #include <io.h>
    #include <fcntl.h>
    #include <share.h>

    #include <sys/stat.h>

    #include <cstdio>
    #include <filesystem>
#else
    #include <fcntl.h>
    #include <unistd.h>
#endif

#include <string>
#include <cstdint>
#include <optional>

namespace
{

// Python wraps the descriptor each standard stream holds (CPython's init_sys_streams takes fileno(stdin)), as the
// start's mode guard reads it, so a case needs its stream on descriptor 0 or 2. A Windows program on another C runtime
// than the interpreter's closes a descriptor Python never reads, so there the cases pin the evaluation alone.
void skip_unless_first_start_reads(std::int32_t descriptor)
{
    if(Py_IsInitialized() != 0)
        SKIP("this case needs a process of its own, whose first evaluation starts the interpreter");
#if defined(_WIN32)
    if(descriptor == 0 && _fileno(stdin) != 0)
        SKIP("the standard input's stream holds no descriptor 0, so neither Python nor the start reads it");
    if(descriptor == 2 && _fileno(stderr) != 2)
        SKIP("the standard error's stream holds no descriptor 2, so neither Python nor the start reads it");
#else
    (void)descriptor;
#endif
}

bool moved_onto(std::int32_t opened, std::int32_t descriptor)
{
#if defined(_WIN32)
    const bool pointed = _dup2(opened, descriptor) != -1;
    _close(opened);
#else
    const bool pointed = dup2(opened, descriptor) != -1;
    close(opened);
#endif
    return pointed;
}

bool standard_input_on_null_device()
{
#if defined(_WIN32)
    const std::filesystem::path null_device("NUL");
    std::int32_t opened = -1;
    if(_wsopen_s(&opened, null_device.c_str(), _O_RDONLY | _O_TEXT, _SH_DENYNO, _S_IREAD) != 0)
        return false;
#else
    const std::int32_t opened = open("/dev/null", O_RDONLY);
    if(opened < 0)
        return false;
#endif
    return opened == 0 || moved_onto(opened, 0);
}

bool standard_input_closed()
{
#if defined(_WIN32)
    return _close(0) == 0;
#else
    return close(0) == 0;
#endif
}

std::optional<std::int32_t> saved_and_closed(std::int32_t descriptor)
{
#if defined(_WIN32)
    const std::int32_t saved = _dup(descriptor);
    const bool closed        = saved >= 0 && _close(descriptor) == 0;
#else
    const std::int32_t saved = dup(descriptor);
    const bool closed        = saved >= 0 && close(descriptor) == 0;
#endif
    return closed ? std::optional<std::int32_t>(saved) : std::nullopt;
}

#if !defined(_WIN32) || MEIOS_KEEPS_STREAM_MODES
std::optional<bool> closed_as_python_finds(std::int32_t descriptor)
{
    pybind11::gil_scoped_acquire held;
    try
    {
        pybind11::module_::import("os").attr("fstat")(descriptor);
        return false;
    }
    catch(const pybind11::error_already_set &raised)
    {
        return raised.matches(PyExc_OSError);
    }
}
#else
std::optional<bool> closed_as_python_finds(std::int32_t)
{
    return std::nullopt;
}
#endif

}

TEST_CASE("the first evaluation runs where the program closed its standard input", "[eval_python][closed_input]")
{
    skip_unless_first_start_reads(0);
    REQUIRE(standard_input_on_null_device());
    REQUIRE(standard_input_closed());
    REQUIRE(evaluate("1 + 1", meios::eval_scope{}) == std::optional<std::string>("2"));
}

TEST_CASE("the first evaluation runs where the program closed its standard error", "[eval_python][closed_error]")
{
    skip_unless_first_start_reads(2);
    const std::optional<std::int32_t> saved = saved_and_closed(2);
    const std::optional<std::string> value  = evaluate("1 + 1", meios::eval_scope{});
    const std::optional<bool> stayed_closed = closed_as_python_finds(2);
    const bool restored                     = saved.has_value() && moved_onto(*saved, 2);
    REQUIRE(saved.has_value());
    REQUIRE(restored);
    REQUIRE(value == std::optional<std::string>("2"));
    if(stayed_closed.has_value())
        CHECK(*stayed_closed);
}

TEST_CASE("the first evaluation runs where a module the start imports closes the standard input", "[eval_python][.closed_during_start]")
{
    skip_unless_first_start_reads(0);
    REQUIRE(standard_input_on_null_device());
    REQUIRE(evaluate("1 + 1", meios::eval_scope{}) == std::optional<std::string>("2"));
    pybind11::gil_scoped_acquire held;
    CHECK(pybind11::module_::import("sys").attr("modules").contains("sitecustomize"));
    CHECK_THROWS_AS(pybind11::module_::import("os").attr("fstat")(0), pybind11::error_already_set);
}
