#include "eval_python_fixture.h"

#include <pybind11/embed.h>

#include <catch2/catch_test_macros.hpp>

#if defined(_WIN32)
    #include <io.h>
    #include <fcntl.h>
    #include <share.h>
    #include <sys/stat.h>
#else
    #include <fcntl.h>
    #include <unistd.h>
#endif

#include <string>
#include <cstdint>
#include <optional>
#include <filesystem>

namespace
{

// Python wraps descriptor 0 itself, not the stream, so the descriptor is what is pointed at the null device.
bool standard_input_on_null_device()
{
#if defined(_WIN32)
    const std::filesystem::path null_device("NUL");
    std::int32_t opened = -1;
    if(_wsopen_s(&opened, null_device.c_str(), _O_RDONLY | _O_TEXT, _SH_DENYNO, _S_IREAD) != 0)
        return false;
    const bool pointed = _dup2(opened, 0) != -1;
    _close(opened);
#else
    const std::int32_t opened = open("/dev/null", O_RDONLY);
    if(opened < 0)
        return false;
    const bool pointed = dup2(opened, 0) != -1;
    close(opened);
#endif
    return pointed;
}

void close_standard_input()
{
#if defined(_WIN32)
    _close(0);
#else
    close(0);
#endif
}

}

TEST_CASE("the first evaluation runs where the program closed its standard input", "[eval_python][closed_input]")
{
    if(Py_IsInitialized() != 0)
        SKIP("this case needs a process of its own, whose first evaluation starts the interpreter");
    REQUIRE(standard_input_on_null_device());
    close_standard_input();
    REQUIRE(evaluate("1 + 1", meios::eval_scope{}) == std::optional<std::string>("2"));
}

TEST_CASE("the first evaluation runs where a module the start imports closes the standard input", "[eval_python][.closed_during_start]")
{
    if(Py_IsInitialized() != 0)
        SKIP("this case needs a process of its own, whose first evaluation starts the interpreter");
    REQUIRE(standard_input_on_null_device());
    REQUIRE(evaluate("1 + 1", meios::eval_scope{}) == std::optional<std::string>("2"));
    pybind11::gil_scoped_acquire held;
    CHECK_THROWS_AS(pybind11::module_::import("os").attr("fstat")(0), pybind11::error_already_set);
}
