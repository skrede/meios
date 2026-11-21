#include "eval_python_fixture.h"

#include <pybind11/embed.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <clocale>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <optional>

#if defined(_WIN32)

    #include <io.h>
    #include <cstdio>
    #include <fcntl.h>

#endif

// Each case relies on its own first evaluation starting the interpreter, which holds because ctest
// runs every discovered case in a process of its own.
namespace
{

using disposition = decltype(std::signal(SIGINT, SIG_DFL));

struct host_state
{
    std::string ctype;
    std::string all;
    std::optional<std::string> exported;
#if defined(_WIN32)
    std::int32_t output;
#endif
};

std::string locale_of(std::int32_t category)
{
    // The returned text lives in a buffer the next call reuses, so it is copied at once.
    const char *const named = std::setlocale(category, nullptr);
    return named != nullptr ? std::string(named) : std::string();
}

std::optional<std::string> variable(const char *name)
{
    const char *const found = std::getenv(name);
    if(found == nullptr)
        return std::nullopt;
    return std::string(found);
}

#if defined(_WIN32)
std::int32_t output_mode()
{
    const std::int32_t mode = _setmode(_fileno(stdout), _O_TEXT);
    _setmode(_fileno(stdout), mode);
    return mode;
}
#else
void clear_locale_environment()
{
    unsetenv("LC_ALL");
    unsetenv("LC_CTYPE");
    unsetenv("LANG");
}
#endif

host_state read_host_state()
{
#if defined(_WIN32)
    return host_state{locale_of(LC_CTYPE), locale_of(LC_ALL), variable("LC_CTYPE"), output_mode()};
#else
    return host_state{locale_of(LC_CTYPE), locale_of(LC_ALL), variable("LC_CTYPE")};
#endif
}

}

TEST_CASE("the first evaluation leaves the host's locale and its locale environment as they were", "[eval_python][host_locale]")
{
    if(Py_IsInitialized() != 0)
        SKIP("this case needs a process of its own, whose first evaluation starts the interpreter");
#if !defined(_WIN32)
    clear_locale_environment();
#endif
    const host_state before = read_host_state();
    REQUIRE(evaluate("1 + 1", meios::eval_scope{}) == std::optional<std::string>("2"));
    const host_state after = read_host_state();
    INFO("LC_CTYPE: " << before.ctype << " before, " << after.ctype << " after");
    INFO("LC_ALL: " << before.all << " before, " << after.all << " after");
    INFO("LC_CTYPE in the environment: " << before.exported.value_or("unset") << " before, " << after.exported.value_or("unset") << " after");
    CHECK(after.ctype == before.ctype);
    CHECK(after.all == before.all);
    CHECK(after.exported == before.exported);
#if defined(_WIN32)
    INFO("standard output mode: " << before.output << " before, " << after.output << " after");
    CHECK(after.output == before.output);
#endif
}

TEST_CASE("the first evaluation leaves the interrupt and broken-pipe dispositions as the host set them", "[eval_python]")
{
    const disposition interrupt = std::signal(SIGINT, SIG_DFL);
#ifdef SIGPIPE
    const disposition pipe = std::signal(SIGPIPE, SIG_DFL);
#endif
    REQUIRE(evaluate("1 + 1", meios::eval_scope{}) == std::optional<std::string>("2"));
    CHECK(std::signal(SIGINT, interrupt) == SIG_DFL);
#ifdef SIGPIPE
    CHECK(std::signal(SIGPIPE, pipe) == SIG_DFL);
#endif
}
