#include "eval_python_fixture.h"
#include "eval_python_descriptor_mode.h"

#include <pybind11/embed.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <clocale>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <optional>

#if defined(_WIN32)

    #include <array>
    #include <cstdio>

#endif

// Each case relies on its own first evaluation starting the interpreter, which holds because ctest
// runs every discovered case in a process of its own.
namespace
{

using disposition = decltype(std::signal(SIGINT, SIG_DFL));

std::vector<std::int32_t> fatal_signals()
{
#ifdef SIGBUS
    return {SIGSEGV, SIGFPE, SIGABRT, SIGILL, SIGBUS};
#else
    return {SIGSEGV, SIGFPE, SIGABRT, SIGILL};
#endif
}

struct host_state
{
    std::string ctype;
    std::string all;
    std::optional<std::string> exported;
#if defined(_WIN32)
    std::array<std::int32_t, 3> modes;
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

#if !defined(_WIN32)
void clear_locale_environment()
{
    unsetenv("LC_ALL");
    unsetenv("LC_CTYPE");
    unsetenv("LANG");
}

// The probe puts the locale back before the case reads it, so the state compared is the one the process started with.
std::optional<std::string> installed_utf8_locale()
{
    const std::string recorded = locale_of(LC_CTYPE);
    std::optional<std::string> accepted;
    for(const char *const candidate : {"C.UTF-8", "en_US.UTF-8", "UTF-8"})
    {
        if(std::setlocale(LC_CTYPE, candidate) != nullptr)
        {
            accepted = candidate;
            break;
        }
    }
    std::setlocale(LC_CTYPE, recorded.c_str());
    return accepted;
}
#endif

host_state read_host_state()
{
#if defined(_WIN32)
    const std::array<std::int32_t, 3> modes{
            descriptor_mode(_fileno(stdin)),
            descriptor_mode(_fileno(stdout)),
            descriptor_mode(_fileno(stderr)),
    };
    return host_state{locale_of(LC_CTYPE), locale_of(LC_ALL), variable("LC_CTYPE"), modes};
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
    const std::array<const char *, 3> streams{"input", "output", "error"};
    for(std::size_t index = 0; index < streams.size(); ++index)
    {
        INFO("standard " << streams[index] << " mode: " << before.modes[index] << " before, " << after.modes[index] << " after");
        CHECK(after.modes[index] == before.modes[index]);
    }
#endif
}

#if !defined(_WIN32)
TEST_CASE("the first evaluation leaves the host's locale alone under a named locale environment", "[eval_python][host_locale_named]")
{
    if(Py_IsInitialized() != 0)
        SKIP("this case needs a process of its own, whose first evaluation starts the interpreter");
    clear_locale_environment();
    const std::optional<std::string> named = installed_utf8_locale();
    if(!named)
        SKIP("no named UTF-8 locale is installed");
    setenv("LC_CTYPE", named->c_str(), 1);
    const host_state before = read_host_state();
    REQUIRE(evaluate("1 + 1", meios::eval_scope{}) == std::optional<std::string>("2"));
    const host_state after = read_host_state();
    INFO("named locale in the environment: " << *named);
    INFO("LC_CTYPE: " << before.ctype << " before, " << after.ctype << " after");
    INFO("LC_ALL: " << before.all << " before, " << after.all << " after");
    INFO("LC_CTYPE in the environment: " << before.exported.value_or("unset") << " before, " << after.exported.value_or("unset") << " after");
    CHECK(after.ctype == before.ctype);
    CHECK(after.all == before.all);
    CHECK(after.exported == before.exported);
}
#endif

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

TEST_CASE("the first evaluation leaves the fatal-signal dispositions as the host set them", "[eval_python][fatal_signals]")
{
    if(Py_IsInitialized() != 0)
        SKIP("this case needs a process of its own, whose first evaluation starts the interpreter");
    const std::vector<std::int32_t> signals = fatal_signals();
    std::vector<disposition> previous;
    for(const std::int32_t number : signals)
        previous.push_back(std::signal(number, SIG_DFL));
    REQUIRE(evaluate("1 + 1", meios::eval_scope{}) == std::optional<std::string>("2"));
    for(std::size_t index = 0; index < signals.size(); ++index)
    {
        INFO("signal " << signals[index]);
        CHECK(std::signal(signals[index], previous[index]) == SIG_DFL);
    }
}
