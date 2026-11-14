#include "meios/xacro/literal_read.h"
#include "meios/xacro/boolean_value.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <string_view>

using meios::value_kind;
using meios::detail::integer_literal;

namespace
{

struct heard
{
    std::vector<meios::diagnostic_code> codes;
    std::vector<std::string> messages;

    void operator()(meios::level, const std::string &) {}

    void operator()(meios::level, const meios::source_location &, const std::string &) {}

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &,
                    const std::string &message)
    {
        if(lvl != meios::level::error)
            return;
        codes.push_back(code);
        messages.push_back(message);
    }
};

struct outcome
{
    meios::eval_failure_kind failure;
    meios::value result;
    heard log;
};

outcome evaluate(std::string_view expression)
{
    heard log;
    meios::log_sink_f sink{ std::ref(log) };
    const meios::eval_scope scope;
    meios::core_evaluator evaluator;
    const meios::value result = evaluator.eval(expression, scope, sink);
    return outcome{ evaluator.failed() ? evaluator.failure_kind() : meios::eval_failure_kind::none,
                    result, log };
}

std::string rendered(std::string_view expression)
{
    const outcome out = evaluate(expression);
    REQUIRE(out.failure == meios::eval_failure_kind::none);
    return meios::render_scalar(out.result).value_or(std::string());
}

const std::string wide_digits = "99999999999999999999";

}

TEST_CASE("an integer literal wider than 64 bits is a wide integer that renders its digits", "[xacro][wide]")
{
    const outcome out = evaluate(wide_digits);
    REQUIRE(out.failure == meios::eval_failure_kind::none);
    CHECK(out.result.kind() == value_kind::wide_integer);
    CHECK(meios::render_scalar(out.result) == std::optional<std::string>(wide_digits));
}

TEST_CASE("integer digits read as an integer inside the 64-bit range and as a wide integer outside it", "[xacro][wide]")
{
    for(const char *text : {"9223372036854775808", "-9223372036854775809"})
    {
        const std::optional<meios::value> read = integer_literal(text);
        REQUIRE(read);
        CHECK(read->kind() == value_kind::wide_integer);
        CHECK(read->wide()->spelling() == text);
    }
    CHECK(integer_literal("9223372036854775807") == meios::value{ std::int64_t{ 9223372036854775807 } });
    CHECK(integer_literal("-9223372036854775808") == meios::value{ std::int64_t{ -9223372036854775807 - 1 } });
    CHECK(integer_literal("0042") == meios::value{ std::int64_t{ 42 } });
    CHECK(integer_literal("-0") == meios::value{ std::int64_t{ 0 } });
    CHECK(integer_literal("000" + wide_digits)->wide()->spelling() == wide_digits);
    for(const char *text : {"+5", "5a", "", "-", "1.0"})
        CHECK_FALSE(integer_literal(text));
}

TEST_CASE("a wide integer holds only a canonical spelling outside the 64-bit range", "[xacro][wide]")
{
    using meios::detail::wide_integer;
    CHECK(wide_integer::from_spelling("-" + wide_digits)->negative());
    CHECK_FALSE(wide_integer::from_spelling(wide_digits)->negative());
    const std::vector<std::string> refused{ "0" + wide_digits, "-0", "0", "42", "-9223372036854775808", "+" + wide_digits };
    for(const std::string &text : refused)
        CHECK_FALSE(wide_integer::from_spelling(text));
    CHECK(wide_integer::from_spelling(std::string(meios::detail::wide_digit_ceiling, '9')));
    CHECK_FALSE(wide_integer::from_spelling(std::string(meios::detail::wide_digit_ceiling + 1, '9')));
}

TEST_CASE("a document names a link by a literal integer wider than 64 bits", "[xacro][wide]")
{
    heard log;
    meios::log_sink_f sink{ std::ref(log) };
    const std::string source = "<robot name=\"wide\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
                               "<link name=\"l_${" + wide_digits + "}\"/></robot>";
    meios::eval_scope scope;
    meios::source_stack sources;
    const meios::expected<meios::expansion, meios::expansion_error> out =
        meios::expand(source, scope, sources, "wide.urdf.xacro", meios::expansion_limits{}, sink);
    REQUIRE(out.has_value());
    CHECK(out->document.find("name=\"l_" + wide_digits + "\"") != std::string::npos);
}

TEST_CASE("a wide integer is named an integer and reads as true", "[xacro][wide]")
{
    CHECK(meios::kind_name(value_kind::wide_integer) == "integer");
    const std::optional<meios::value> read = integer_literal(wide_digits);
    REQUIRE(read);
    CHECK(meios::detail::get_boolean_value(*read) == std::optional<bool>(true));
}

TEST_CASE("a wide integer is true wherever the evaluator reads a truth", "[xacro][wide]")
{
    CHECK(rendered("not " + wide_digits) == "False");
    CHECK(rendered(wide_digits + " and 5") == "5");
    CHECK(rendered("0 or " + wide_digits) == wide_digits);
    CHECK(rendered("1 if " + wide_digits + " else 0") == "1");
}

TEST_CASE("an integer literal holds at most the digit ceiling and a longer one refuses naming it", "[xacro][wide]")
{
    const std::string longest(meios::detail::wide_digit_ceiling, '9');
    CHECK(rendered(longest) == longest);
    const outcome out = evaluate(longest + "9");
    CHECK(out.failure == meios::eval_failure_kind::error);
    REQUIRE(out.log.messages.size() == 1);
    CHECK(out.log.messages[0].find("4300") != std::string::npos);
    CHECK(out.log.messages[0].find("99999") == std::string::npos);
}

TEST_CASE("arithmetic, a sign, ordering, numeric equality, min, max and abs on a wide integer refuse as unsupported", "[xacro][wide]")
{
    for(const std::string &expression : { wide_digits + " + 1", wide_digits + " * 2", "-" + wide_digits, "+" + wide_digits,
                                          wide_digits + " > 0", wide_digits + " == " + wide_digits, "min(" + wide_digits + ", 1)",
                                          "max(1, " + wide_digits + ")", "abs(" + wide_digits + ")", wide_digits + " ** 2",
                                          wide_digits + " // 7", "floor(" + wide_digits + ")" })
    {
        DYNAMIC_SECTION(expression)
        {
            const outcome out = evaluate(expression);
            CHECK(out.failure == meios::eval_failure_kind::unsupported);
            REQUIRE(out.log.messages.size() == 1);
            CHECK(out.log.messages[0].find("outside the 64-bit range") != std::string::npos);
        }
    }
}

TEST_CASE("a wide integer equals no string and concatenates with none", "[xacro][wide]")
{
    CHECK(rendered(wide_digits + " == 'x'") == "False");
    CHECK(rendered(wide_digits + " != 'x'") == "True");
    CHECK(evaluate("'x' + " + wide_digits).failure == meios::eval_failure_kind::unsupported);
}

TEST_CASE("a wide sequence index refuses as an index outside the sequence", "[xacro][wide]")
{
    const outcome out = evaluate("'a,b'.split(',')[" + wide_digits + "]");
    CHECK(out.failure == meios::eval_failure_kind::error);
    REQUIRE(out.log.codes.size() == 1);
    CHECK(out.log.codes[0] == meios::diagnostic_code::undefined_property);
    CHECK(out.log.messages[0].find(wide_digits + " is outside the sequence") != std::string::npos);
}
