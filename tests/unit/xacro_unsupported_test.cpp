#include <meios/xacro.h>

#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <functional>
#include <string_view>

namespace
{

struct recorder
{
    int diagnostics{ 0 };
    meios::level worst{ meios::level::info };
    meios::diagnostic_code code{ meios::diagnostic_code::unspecified };
    std::string message{};

    void note(meios::level severity, const std::string &text)
    {
        message = text;
        if(severity <= worst)
            worst = severity;
        ++diagnostics;
    }

    void operator()(meios::level severity, const std::string &text) { note(severity, text); }

    void operator()(meios::level severity, const meios::source_location &, const std::string &text)
    {
        note(severity, text);
    }

    void operator()(meios::level severity, meios::diagnostic_code found,
                    const meios::source_location &, const std::string &text)
    {
        code = found;
        note(severity, text);
    }
};

recorder run(std::string_view expression, bool &failed)
{
    recorder log;
    meios::core_evaluator evaluator;
    meios::eval_scope scope;
    meios::log_sink_f sink{ std::ref(log) };
    evaluator.eval(expression, scope, sink);
    failed = evaluator.failed();
    return log;
}

}

TEST_CASE("out-of-subset forms loud-fail with an error diagnostic", "[xacro][unsupported]")
{
    const std::string_view forms[] = {
        "[x for x in y]",
        "[1, 2, 3]",
        "{'a': 1}",
        "load_yaml('robot.yaml')",
        "foo(1, 2)",
        "python::eval",
    };

    for(std::string_view form : forms)
    {
        bool failed = false;
        const recorder log = run(form, failed);
        INFO("form: " << form);
        REQUIRE(failed);
        REQUIRE(log.diagnostics >= 1);
        REQUIRE(log.worst == meios::level::error);
    }
}

// Two strings added are inside the subset now that the meaning is measured; a string beside any
// other kind is still outside it, and refuses rather than being given an invented coercion.
TEST_CASE("two strings concatenate where a string beside another kind still loud-fails",
          "[xacro][unsupported]")
{
    bool failed = true;
    const recorder joined = run("'left' + 'right'", failed);
    REQUIRE_FALSE(failed);
    REQUIRE(joined.diagnostics == 0);

    const recorder mixed = run("'left' + 1", failed);
    REQUIRE(failed);
    REQUIRE(mixed.worst == meios::level::error);
}

// The mapping constructor is the one Python constructor an expression here builds a value from,
// and only in its keyword-argument spelling: that spelling is measured against the reference and
// recorded, while every other argument shape and every other constructor name is refused rather
// than given a meaning nothing measured.
TEST_CASE("a keyword-argument mapping constructor builds a value the evaluator can subscript",
          "[xacro][unsupported]")
{
    bool failed = true;
    const recorder log = run("dict(a=1, b=2)['b']", failed);
    REQUIRE_FALSE(failed);
    REQUIRE(log.diagnostics == 0);
}

TEST_CASE("a mapping constructor argument that is not a keyword loud-fails naming the form",
          "[xacro][unsupported]")
{
    const std::string_view shapes[] = { "dict({'a': 1})", "dict([('a', 1)])",
                                        "dict([('a', 1)], b=2)", "dict(a=1, 2)" };

    for(std::string_view form : shapes)
    {
        bool failed = false;
        const recorder log = run(form, failed);
        INFO("form: " << form);
        REQUIRE(failed);
        REQUIRE(log.worst == meios::level::error);
        REQUIRE(log.code == meios::diagnostic_code::unsupported_expression);
        REQUIRE(log.message.find("keyword arguments only") != std::string::npos);
    }
}

TEST_CASE("the remaining Python constructor calls loud-fail naming the construct",
          "[xacro][unsupported]")
{
    const std::string_view names[] = { "list", "set", "tuple" };

    for(std::string_view name : names)
    {
        bool failed = false;
        const recorder log = run(std::string(name) + "(1)", failed);
        INFO("constructor: " << name);
        REQUIRE(failed);
        REQUIRE(log.diagnostics >= 1);
        REQUIRE(log.worst == meios::level::error);
        REQUIRE(log.code == meios::diagnostic_code::unsupported_expression);
        REQUIRE(log.message.find(name) != std::string::npos);
    }
}

TEST_CASE("a name absent from scope loud-fails rather than defaulting", "[xacro][unsupported]")
{
    bool failed = false;
    const recorder log = run("missing_property * 2", failed);
    REQUIRE(failed);
    REQUIRE(log.diagnostics >= 1);
    REQUIRE(log.worst == meios::level::error);
}

TEST_CASE("a supported expression does not raise a diagnostic", "[xacro][unsupported]")
{
    bool failed = true;
    const recorder log = run("1 + 2 * 3", failed);
    REQUIRE_FALSE(failed);
    REQUIRE(log.diagnostics == 0);
}
