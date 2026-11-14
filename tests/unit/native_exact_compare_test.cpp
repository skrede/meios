#include "meios/xacro/literal_read.h"
#include "meios/xacro/numeric_order.h"

#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>
#include <vector>
#include <compare>
#include <utility>
#include <optional>
#include <functional>
#include <string_view>

using meios::detail::numeric_order;

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

meios::value read(std::string_view digits)
{
    return *meios::detail::integer_literal(digits);
}

meios::value real(double number)
{
    return *meios::value::make_real(number);
}

meios::eval_failure_kind failure_of(std::string_view expression)
{
    heard log;
    meios::log_sink_f sink{ std::ref(log) };
    meios::eval_scope scope;
    meios::core_evaluator evaluator;
    evaluator.eval(expression, scope, sink);
    return evaluator.failed() ? evaluator.failure_kind() : meios::eval_failure_kind::none;
}

std::string rendered(std::string_view expression)
{
    heard log;
    meios::log_sink_f sink{ std::ref(log) };
    meios::eval_scope scope;
    scope.set("w", read("99999999999999999999"));
    scope.set("n", read("-99999999999999999999"));
    scope.set("m", read("-99999999999999999998"));
    meios::core_evaluator evaluator;
    const meios::value result = evaluator.eval(expression, scope, sink);
    REQUIRE_FALSE(evaluator.failed());
    return meios::render_scalar(result).value_or(std::string());
}

const std::array<std::pair<const char *, const char *>, 17> measured{ {
        {"9007199254740993 > 9007199254740992", "True"}, {"9007199254740993 > 9007199254740992.0", "True"},
        {"9007199254740993 == 9007199254740992", "False"}, {"9223372036854775807 < 9223372036854775808.0", "True"},
        {"99999999999999999999 > 99999999999999999998", "True"}, {"n < m", "True"},
        {"99999999999999999999 < 1e20", "True"}, {"99999999999999999999 == 1e20", "False"},
        {"100000000000000000000 == 1e20", "True"}, {"n > -1e20", "True"}, {"99999999999999999999 > 1.5", "True"},
        {"True < 99999999999999999999", "True"}, {"0 < 99999999999999999999 < 1e21", "True"},
        {"w == 99999999999999999999", "True"}, {"n < -9223372036854775807", "True"}, {"n < -1.5", "True"},
        {"-9223372036854775807 - 1 == -9223372036854775808.0", "True"},
} };

}

TEST_CASE("comparing two numbers is exact at every width and between every pair of kinds", "[xacro][wide]")
{
    for(const std::pair<const char *, const char *> &row : measured)
    {
        INFO(row.first);
        CHECK(rendered(row.first) == row.second);
    }
}

TEST_CASE("the exact order of a wide integer and a real at or past 2^63 reads the real's digits", "[xacro][wide]")
{
    CHECK(numeric_order(read("9223372036854775808"), real(9223372036854775808.0)) == std::partial_ordering::equivalent);
    CHECK(numeric_order(read("9223372036854775809"), real(9223372036854775808.0)) == std::partial_ordering::greater);
    CHECK(numeric_order(read("-9223372036854775809"), real(-9223372036854775808.0)) == std::partial_ordering::less);
    CHECK(numeric_order(real(1e300), read(std::string(300, '9'))) == std::partial_ordering::greater);
    CHECK(numeric_order(read("-" + std::string(310, '9')), real(-1.7976931348623157e308)) == std::partial_ordering::less);
    CHECK(numeric_order(meios::value{ true }, read("1")) == std::partial_ordering::equivalent);
    CHECK_FALSE(numeric_order(meios::value{ std::string("1") }, read("1")));
    CHECK_FALSE(numeric_order(meios::value{}, read("99999999999999999999")));
}

TEST_CASE("min and max order exactly and answer the first of equal arguments", "[xacro][wide]")
{
    CHECK(rendered("min(99999999999999999999, 1e20)") == "99999999999999999999");
    CHECK(rendered("max(99999999999999999999, 1e20)") == "1e+20");
    CHECK(rendered("min(1, 1.0)") == "1");
    CHECK(rendered("max(1.0, 1)") == "1.0");
    CHECK(rendered("min(2, 1.0, 1)") == "1.0");
    CHECK(rendered("max(n, m, -1e20)") == "-99999999999999999998");
}

TEST_CASE("a wide integer beside a string compares only for equality", "[xacro][wide]")
{
    CHECK(rendered("99999999999999999999 == '99999999999999999999'") == "False");
    CHECK(failure_of("99999999999999999999 < 'a'") == meios::eval_failure_kind::unsupported);
}
