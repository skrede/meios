#include "eval_python_fixture.h"

#include "module_path.h"

#include <pybind11/embed.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <random>
#include <string>
#include <cstdint>
#include <csignal>
#include <fstream>
#include <optional>
#include <functional>
#include <filesystem>
#include <string_view>
#include <system_error>

// Each case relies on its own first evaluation starting the interpreter, which holds because ctest
// runs every discovered case in a process of its own. The failing starts are hidden, since they need
// the environment their own registration sets.
namespace
{

using disposition = decltype(std::signal(SIGINT, SIG_DFL));

struct planted_directory
{
    planted_directory()
            : m_root(std::filesystem::temp_directory_path() / ("meios-start-" + std::to_string(std::random_device{}())))
            , m_left(std::filesystem::current_path())
    {
        std::filesystem::create_directories(m_root);
        std::ofstream(m_root / "ast.py") << "open(__file__ + '.ran', 'w').close()\n";
        std::filesystem::current_path(m_root);
    }

    ~planted_directory()
    {
        std::error_code ignored;
        std::filesystem::current_path(m_left, ignored);
        std::filesystem::remove_all(m_root, ignored);
    }

    planted_directory(const planted_directory &)            = delete;
    planted_directory &operator=(const planted_directory &) = delete;

    std::filesystem::path m_root;
    std::filesystem::path m_left;
};

}

TEST_CASE("the first evaluation reads no module from the directory the process was started in", "[eval_python][planted_module]")
{
    const planted_directory planted;
    REQUIRE(evaluate("1 + 1", meios::eval_scope{}) == std::optional<std::string>("2"));
    pybind11::gil_scoped_acquire gil;
    pybind11::dict space;
    space["start"] = planted.m_root.string();
    pybind11::exec(R"(
import os, sys
here = {os.path.realpath(start), os.path.realpath(os.getcwd())}
on_path = [entry for entry in sys.path if entry == '' or os.path.realpath(entry) in here]
listed = [entry for entry in os.environ.get('PYTHONPATH', '').split(os.pathsep) if entry]
known = {os.path.normcase(os.path.realpath(entry)) for entry in sys.path}
missing = [entry for entry in listed if os.path.normcase(os.path.realpath(entry)) not in known]
import ast
planted = os.path.realpath(getattr(ast, '__file__', '')).startswith(os.path.realpath(start))
)",
                   space);
    INFO("sys.path: " << pybind11::str(pybind11::module_::import("sys").attr("path")).cast<std::string>());
    INFO("missing: " << pybind11::str(space["missing"]).cast<std::string>());
    CHECK(pybind11::len(space["on_path"]) == 0);
    CHECK(pybind11::len(space["missing"]) == 0);
    CHECK_FALSE(space["planted"].cast<bool>());
    CHECK_FALSE(std::filesystem::exists(planted.m_root / "ast.py.ran"));
}

TEST_CASE("a module path list keeps every named element in its order and drops only the empty ones", "[eval_python][module_path]")
{
    using meios::detail::without_empty_elements;
    CHECK(without_empty_elements<char>("", ':').empty());
    CHECK(without_empty_elements<char>(":", ':').empty());
    CHECK(without_empty_elements<char>("::", ':').empty());
    CHECK(without_empty_elements<char>("/a", ':') == "/a");
    CHECK(without_empty_elements<char>(":/a", ':') == "/a");
    CHECK(without_empty_elements<char>("/a:", ':') == "/a");
    CHECK(without_empty_elements<char>("/a::/b", ':') == "/a:/b");
    CHECK(without_empty_elements<char>(":/a::/b:", ':') == "/a:/b");
    CHECK(without_empty_elements<char>(".", ':') == ".");
    CHECK(without_empty_elements<wchar_t>(L";C:\\a;;D:\\b;", L';') == L"C:\\a;D:\\b");
}

namespace
{

struct reported_outcome
{
    meios::text_outcome out;
    tally counts;
};

reported_outcome evaluated(std::string_view expr)
{
    meios::python_evaluator evaluator;
    reported_outcome result{meios::text_outcome{std::nullopt, meios::eval_failure_kind::none}, tally{}};
    meios::log_sink_f sink{std::ref(result.counts)};
    result.out = evaluator.eval_to_text(expr, meios::eval_scope{}, sink);
    return result;
}

std::string interpreter_state()
{
    const pybind11::dict space(pybind11::arg("sys") = pybind11::module_::import("sys"));
    return pybind11::str(pybind11::eval("(sys.get_int_max_str_digits(), sys.path)", space)).cast<std::string>();
}

disposition interrupt_disposition()
{
    const disposition found = std::signal(SIGINT, SIG_DFL);
    std::signal(SIGINT, found);
    return found;
}

}

TEST_CASE("an interpreter the host already runs is used as the host left it", "[eval_python][adopted]")
{
    if(Py_IsInitialized() != 0)
        SKIP("this case needs a process of its own, whose interpreter the host starts");
    PyConfig config;
    PyConfig_InitPythonConfig(&config);
    config.install_signal_handlers = 0;
    REQUIRE_FALSE(PyStatus_Exception(Py_InitializeFromConfig(&config)));
    PyConfig_Clear(&config);
    pybind11::module_::import("sys").attr("set_int_max_str_digits")(5000);
    const std::string before    = interpreter_state();
    const disposition interrupt = interrupt_disposition();
    PyThreadState *const host   = PyEval_SaveThread();
    CHECK(evaluate("1 + 1", meios::eval_scope{}) == std::optional<std::string>("2"));
    CHECK(interrupt_disposition() == interrupt);
    PyEval_RestoreThread(host);
    CHECK(interpreter_state() == before);
    CHECK(pybind11::module_::import("sys").attr("get_int_max_str_digits")().cast<std::int64_t>() == 5000);
}

TEST_CASE("an interpreter that cannot start fails every evaluation as an error and is not tried again", "[eval_python][.start_failure]")
{
    for(std::int32_t attempt = 0; attempt < 2; ++attempt)
    {
        const reported_outcome result = evaluated("1 + 1");
        CHECK_FALSE(result.out.text.has_value());
        CHECK(result.out.failure == meios::eval_failure_kind::error);
        CHECK(result.counts.errors == 1);
        CHECK(result.counts.last.find("did not start") != std::string::npos);
    }
    const auto handle      = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const outcome expanded = run(std::string(header) + R"(<link name="l_${1 + 1}"/></robot>)", meios::eval_policy::fail, handle);
    CHECK_FALSE(expanded.expanded.has_value());
    CHECK(expanded.reported.find("did not start") != std::string::npos);
}

TEST_CASE("a digit limit that cannot be held fails the first evaluation only", "[eval_python][.pin_failure]")
{
    const reported_outcome first = evaluated("1 + 1");
    CHECK_FALSE(first.out.text.has_value());
    CHECK(first.out.failure == meios::eval_failure_kind::error);
    CHECK(first.counts.last.find("digit limit") != std::string::npos);
    CHECK(evaluate("1 + 1", meios::eval_scope{}) == std::optional<std::string>("2"));
}
