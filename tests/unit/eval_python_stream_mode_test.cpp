#include "eval_python_fixture.h"
#include "eval_python_descriptor_mode.h"

#include "kept_stream_modes.h"

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

#if MEIOS_KEEPS_STREAM_MODES

namespace
{

struct text_mode_streams
{
    std::optional<std::string> evaluated;
    std::int32_t input_mode;
    std::int32_t error_mode;
    std::string read;
};

// No assertion runs while one holds the output or the error, since the reporter writes there.
class redirected_descriptor
{
public:
    redirected_descriptor(std::int32_t descriptor, const std::filesystem::path &path, std::int32_t flags)
            : m_descriptor(descriptor)
            , m_saved(flushed_duplicate(descriptor))
    {
        std::int32_t file = -1;
        if(_wsopen_s(&file, path.c_str(), flags, _SH_DENYNO, _S_IREAD | _S_IWRITE) != 0)
            return;
        _dup2(file, m_descriptor);
        _close(file);
    }

    ~redirected_descriptor()
    {
        if(m_saved < 0)
            return;
        _dup2(m_saved, m_descriptor);
        _close(m_saved);
    }

    redirected_descriptor(const redirected_descriptor &)            = delete;
    redirected_descriptor &operator=(const redirected_descriptor &) = delete;

private:
    std::int32_t m_descriptor;
    std::int32_t m_saved;

    static std::int32_t flushed_duplicate(std::int32_t descriptor)
    {
        std::cout.flush();
        std::cerr.flush();
        std::fflush(stdout);
        std::fflush(stderr);
        return _dup(descriptor);
    }
};

void python_writes_line(const char *stream)
{
    pybind11::gil_scoped_acquire held;
    const pybind11::object output = pybind11::module_::import("sys").attr(stream);
    output.attr("write")("line\n");
    output.attr("flush")();
}

std::string python_reads_input()
{
    pybind11::gil_scoped_acquire held;
    const pybind11::object input = pybind11::module_::import("sys").attr("stdin").attr("buffer");
    return input.attr("read")().cast<std::string>();
}

void write_bytes(const std::filesystem::path &path, const std::string &bytes)
{
    std::ofstream out(path, std::ios::binary);
    out << bytes;
}

std::string read_bytes(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

text_mode_streams evaluated_over_text_streams(const std::filesystem::path &input, const std::filesystem::path &error)
{
    const redirected_descriptor from(0, input, _O_RDONLY | _O_TEXT);
    const redirected_descriptor to(2, error, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_TEXT);
    text_mode_streams seen{evaluate("1 + 1", meios::eval_scope{}), -1, -1, std::string()};
    if(seen.evaluated)
    {
        seen.input_mode = descriptor_mode(0);
        seen.error_mode = descriptor_mode(2);
        seen.read       = python_reads_input();
        python_writes_line("stderr");
    }
    return seen;
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
        const redirected_descriptor redirected(1, path, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_TEXT);
        evaluated = evaluate("1 + 1", meios::eval_scope{});
        if(evaluated)
        {
            mode = descriptor_mode(1);
            python_writes_line("stdout");
        }
    }
    const std::string bytes = read_bytes(path);
    std::filesystem::remove(path);
    REQUIRE(evaluated == std::optional<std::string>("2"));
    INFO("bytes written: " << bytes.size());
    CHECK(mode == _O_TEXT);
    CHECK(bytes == "line\r\r\n");
}

TEST_CASE("Python reads a standard input and writes a standard error kept in text mode through the runtime's text translation", "[eval_python][stream_mode]")
{
    if(Py_IsInitialized() != 0)
        SKIP("this case needs a process of its own, whose first evaluation starts the interpreter");
    const std::filesystem::path directory = std::filesystem::temp_directory_path();
    const std::filesystem::path input     = directory / "meios_stream_mode_input.txt";
    const std::filesystem::path error     = directory / "meios_stream_mode_error.txt";
    write_bytes(input, std::string{'a', '\r', '\n', 'b', '\x1a', 'c'});
    const text_mode_streams seen = evaluated_over_text_streams(input, error);
    const std::string bytes      = read_bytes(error);
    std::filesystem::remove(input);
    std::filesystem::remove(error);
    REQUIRE(seen.evaluated == std::optional<std::string>("2"));
    INFO("bytes written: " << bytes.size());
    CHECK(seen.input_mode == _O_TEXT);
    CHECK(seen.error_mode == _O_TEXT);
    CHECK(seen.read == "a\nb");
    CHECK(bytes == "line\r\r\n");
}

TEST_CASE("a standard output the program put in a wide-character mode is left in binary mode by the first evaluation", "[eval_python][stream_mode]")
{
    if(Py_IsInitialized() != 0)
        SKIP("this case needs a process of its own, whose first evaluation starts the interpreter");
    const std::filesystem::path path = std::filesystem::temp_directory_path() / "meios_stream_mode_wide.txt";
    std::optional<std::string> evaluated;
    std::int32_t previous = -1;
    std::int32_t mode     = -1;
    {
        const redirected_descriptor redirected(1, path, _O_WRONLY | _O_CREAT | _O_TRUNC | _O_TEXT);
        previous  = _setmode(1, _O_U16TEXT);
        evaluated = evaluate("1 + 1", meios::eval_scope{});
        mode      = descriptor_mode(1);
    }
    std::filesystem::remove(path);
    REQUIRE(evaluated == std::optional<std::string>("2"));
    CHECK(previous == _O_TEXT);
    CHECK(mode == _O_BINARY);
}

#endif
