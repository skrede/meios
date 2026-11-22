#include "eval_python_fixture.h"

#include <pybind11/embed.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <filesystem>

#include <io.h>
#include <cstdio>
#include <fcntl.h>
#include <share.h>
#include <sys/stat.h>

namespace
{

// No assertion runs while one is alive, since the reporter writes to the descriptor it holds.
class redirected_output
{
public:
    explicit redirected_output(const std::filesystem::path &path)
            : m_saved(flushed_duplicate())
    {
        std::int32_t file = -1;
        if(_wsopen_s(&file, path.c_str(), _O_WRONLY | _O_CREAT | _O_TRUNC | _O_TEXT, _SH_DENYNO, _S_IREAD | _S_IWRITE) != 0)
            return;
        _dup2(file, 1);
        _close(file);
    }

    ~redirected_output()
    {
        if(m_saved < 0)
            return;
        _dup2(m_saved, 1);
        _close(m_saved);
    }

    redirected_output(const redirected_output &)            = delete;
    redirected_output &operator=(const redirected_output &) = delete;

private:
    std::int32_t m_saved;

    static std::int32_t flushed_duplicate()
    {
        std::cout.flush();
        std::fflush(stdout);
        return _dup(1);
    }
};

std::int32_t output_mode()
{
    const std::int32_t mode = _setmode(_fileno(stdout), _O_TEXT);
    _setmode(_fileno(stdout), mode);
    return mode;
}

void python_writes_line()
{
    pybind11::gil_scoped_acquire held;
    const pybind11::object output = pybind11::module_::import("sys").attr("stdout");
    output.attr("write")("line\n");
    output.attr("flush")();
}

std::string read_bytes(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

}

TEST_CASE("a line Python writes to a standard output kept in text mode ends with two carriage returns", "[eval_python][stream_mode]")
{
    if(Py_IsInitialized() != 0)
        SKIP("this case needs a process of its own, whose first evaluation starts the interpreter");
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "meios_stream_mode_output.txt";
    std::optional<std::string> evaluated;
    std::int32_t mode = -1;
    {
        const redirected_output redirected(path);
        evaluated = evaluate("1 + 1", meios::eval_scope{});
        if(evaluated)
        {
            mode = output_mode();
            python_writes_line();
        }
    }
    const std::string bytes = read_bytes(path);
    std::filesystem::remove(path);
    REQUIRE(evaluated == std::optional<std::string>("2"));
    INFO("bytes written: " << bytes.size());
    CHECK(mode == _O_TEXT);
    CHECK(bytes == "line\r\r\n");
}
