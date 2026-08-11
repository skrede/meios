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
        "'left' + 'right'",
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

// dict(...) is the one Python-constructor call in the pinned Universal Robots closure, appearing
// only in ros2_control_mock_hardware.xacro and inc/ur_joint_control.xacro -- both outside the
// closure reached from ur.urdf.xacro, so this is the one form the reached surface never measures.
TEST_CASE("a Python dict(...) constructor call loud-fails naming the construct",
          "[xacro][unsupported]")
{
    bool failed = false;
    const recorder log = run("dict(a=1, b=2)", failed);
    REQUIRE(failed);
    REQUIRE(log.diagnostics >= 1);
    REQUIRE(log.worst == meios::level::error);
    REQUIRE(log.code == meios::diagnostic_code::unsupported_expression);
    REQUIRE(log.message.find("dict") != std::string::npos);
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
