#include "meios/xacro/big_integer.h"

#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <optional>
#include <functional>
#include <string_view>

using meios::detail::big_integer;

namespace
{

struct heard
{
    std::vector<std::string> messages;

    void operator()(meios::level, const std::string &) {}

    void operator()(meios::level, const meios::source_location &, const std::string &) {}

    void operator()(meios::level lvl, meios::diagnostic_code, const meios::source_location &, const std::string &message)
    {
        if(lvl == meios::level::error)
            messages.push_back(message);
    }
};

struct outcome
{
    meios::eval_failure_kind failure;
    std::string rendered;
    heard log;
};

outcome evaluate(std::string_view expression)
{
    heard log;
    meios::log_sink_f sink{ std::ref(log) };
    const meios::eval_scope scope;
    meios::core_evaluator evaluator;
    const meios::value result = evaluator.eval(expression, scope, sink);
    return outcome{ evaluator.failed() ? evaluator.failure_kind() : meios::eval_failure_kind::none, meios::render_scalar(result).value_or(std::string()), log };
}

big_integer big(std::string_view spelling)
{
    return *big_integer::from_spelling(spelling);
}

big_integer two_to(std::size_t bits)
{
    return big("1") << bits;
}

void check_renders(const std::pair<std::string, std::string> &expected)
{
    CAPTURE(expected.first);
    const outcome out = evaluate(expected.first);
    CHECK(out.failure == meios::eval_failure_kind::none);
    CHECK(out.rendered == expected.second);
}

const std::string wide = "99999999999999999999";

}

TEST_CASE("the true quotient of two integers is rounded once to the nearest real", "[native][big_integer]")
{
    struct row
    {
        big_integer dividend;
        big_integer divisor;
        double quotient;
    };
    const row rows[] = {
            {big(wide), big("3"), 3.333333333333333e+19}, {big("-" + wide), big("7"), -1.4285714285714287e+19}, {big("7"), big(wide), 7e-20},
            {big(wide), big(wide), 1.0}, {pow(big("2"), 70), big("3"), 3.935305402391371e+20}, {pow(big("10"), 30) + big("1"), big("3"), 3.333333333333333e+29},
            {two_to(64), big("3"), 6.148914691236517e+18}, {two_to(64) + big("1"), big("2"), 9.223372036854776e+18},
            {big("9007199254740993"), big("1"), 9007199254740992.0}, {big("18014398509481987"), big("2"), 9007199254740994.0},
            {big("10384593717069656409982497265287169"), two_to(60), 9007199254740994.0},
            {two_to(1024) - two_to(970) - big("1"), big("1"), std::numeric_limits<double>::max()},
            {big("7"), pow(big("10"), 320), 7e-320}, {big("3"), two_to(1075), 1e-323}, {big("5"), two_to(1076), 5e-324},
            {two_to(59) + big("1"), two_to(1134), 5e-324}, {big("1"), two_to(1075), 0.0}, {big("1"), pow(big("10"), 400), 0.0}};
    for(const row &r : rows)
    {
        CAPTURE(r.dividend.spelling().substr(0, 40), r.divisor.spelling().substr(0, 40));
        CHECK(true_quotient(r.dividend, r.divisor) == std::optional<double>(r.quotient));
    }
}

TEST_CASE("the true quotient keeps the sign of a zero and answers nothing past the double range", "[native][big_integer]")
{
    CHECK(std::signbit(*true_quotient(big("0"), big("-5"))));
    CHECK(std::signbit(*true_quotient(big("-1"), pow(big("10"), 400))));
    CHECK_FALSE(std::signbit(*true_quotient(big("0"), big(wide))));
    CHECK_FALSE(true_quotient(two_to(1024) - two_to(970), big("1")));
    CHECK_FALSE(true_quotient(pow(big("10"), 400), big("1")));
}

TEST_CASE("true division of integers renders the quotient rounded once", "[xacro][wide]")
{
    const std::pair<std::string, std::string> measured[] = {
            {"2 ** 70 / 3", "3.935305402391371e+20"}, {"(10**30 + 1) / 3", "3.333333333333333e+29"}, {"2**64 / 3", "6.148914691236517e+18"},
            {"(2**64 + 1) / 2", "9.223372036854776e+18"}, {"9007199254740993 / 1", "9007199254740992.0"}, {"7 / 10**320", "7e-320"},
            {"1 / 10**400", "0.0"}, {"4 / 2", "2.0"}, {"-7 / 2", "-3.5"}, {"True / 2", "0.5"}};
    for(const auto &expected : measured)
        check_renders(expected);
}

TEST_CASE("a wide integer beside a real reads as the nearest real", "[xacro][wide]")
{
    const std::pair<std::string, std::string> measured[] = {
            {wide + " * 0.5", "5e+19"}, {wide + " + 1.5", "1e+20"}, {wide + " // 2.5", "4e+19"}, {wide + " % 2.5", "0.0"},
            {"sqrt(" + wide + ")", "10000000000.0"}, {wide + " ** 0.5", "10000000000.0"}, {wide + " ** -1", "1e-20"}};
    for(const auto &expected : measured)
        check_renders(expected);
}

TEST_CASE("an integer or a quotient too large for a real refuses", "[xacro][wide]")
{
    for(const char *expression : { "10**400 * 1.0", "1.5 - 10**400", "sqrt(10**400)", "10**400 / 1", "-(10**400) / 3" })
    {
        CAPTURE(expression);
        const outcome out = evaluate(expression);
        CHECK(out.failure == meios::eval_failure_kind::error);
        REQUIRE(out.log.messages.size() == 1);
        CHECK(out.log.messages[0].find("too large for a real") != std::string::npos);
    }
}
