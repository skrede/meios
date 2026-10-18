#include "eval_python_fixture.h"

#include "python_eval_detail.h"

#include <pybind11/embed.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <cstdint>
#include <optional>
#include <functional>
#include <string_view>

namespace
{

outcome under_python(std::string_view declarations, std::string_view inner)
{
    const auto handle    = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    std::string document = span_document(inner);
    document.insert(header.size(), declarations);
    return run(document, meios::eval_policy::fail, handle);
}

// The error is raised as a C extension raises one, so the interpreter attaches no unbound name to
// it, which is what every NameError looks like before Python 3.10.
meios::text_outcome raising(const char *definition, std::string &reported)
{
    meios::eval_scope scope;
    scope.bind_argument("flag", meios::value{std::string("false")});
    tally counts;
    meios::log_sink_f sink{std::ref(counts)};
    const auto raise = [definition](std::string_view, meios::detail::yaml_context &, const std::string &) -> meios::text_outcome
    {
        pybind11::dict space;
        pybind11::exec(definition, space);
        PyErr_SetObject(pybind11::object(space["kind"]).ptr(), pybind11::object(space["raised"]).ptr());
        throw pybind11::error_already_set();
    };
    const meios::text_outcome out = meios::detail::under_interpreter("flag", scope, sink, raise);
    reported                      = counts.every;
    return out;
}
}

TEST_CASE("the evaluation command reads the argument table under the python evaluator", "[eval_python]")
{
    const outcome out =
            under_python(R"XML(<xacro:arg name="p" default="A"/><xacro:arg name="q" default="3"/><xacro:property name="q" value="10"/>)XML", "$(eval p + 'arm')_$(eval q + 1)");
    REQUIRE(leaves(out, "<l>Aarm_4</l>"));
}

TEST_CASE("the evaluation command types an argument and resolves the reference's own names first under the python evaluator", "[eval_python]")
{
    const outcome out = under_python(R"XML(<xacro:arg name="real" default="1.50"/><xacro:arg name="e" default="X"/><xacro:arg name="len" default="L"/>)XML",
                                     "$(eval real)_$(eval e)_$(eval true)_$(eval len)");
    REQUIRE(leaves(out, "<l>1.5_2.718281828459045_True_L</l>"));
}

TEST_CASE("the evaluation command reads no property under the python evaluator, though an expression does", "[eval_python]")
{
    meios::eval_scope scope;
    scope.set("w", meios::value{std::int64_t{2}});
    REQUIRE(evaluate("w + 1", scope) == std::optional<std::string>("3"));
    const outcome refused = under_python(R"XML(<xacro:property name="w" value="2"/>)XML", "$(eval w + 1)");
    REQUIRE_FALSE(refused.expanded.has_value());
    CHECK(refused.reported.find("$(arg") == std::string::npos);
}

TEST_CASE("the argument hint is read from the error itself, whether or not the interpreter names the unbound name", "[eval_python]")
{
    std::string reported;
    const meios::text_outcome out = raising("kind = NameError\nraised = NameError(\"name 'flag' is not defined\")\n", reported);
    REQUIRE(out.failure == meios::eval_failure_kind::error);
    REQUIRE(reported.find("$(arg flag)") != std::string::npos);
}

TEST_CASE("an error whose attributes raise when read still ends on a typed failure", "[eval_python]")
{
    constexpr const char *flaky = "class kind(NameError):\n"
                                  "    reads = 0\n"
                                  "    @property\n"
                                  "    def name(self):\n"
                                  "        kind.reads += 1\n"
                                  "        if kind.reads > 1:\n"
                                  "            raise RuntimeError('unreadable')\n"
                                  "        return 'flag'\n"
                                  "raised = kind(\"name 'flag' is not defined\")\n";
    std::string reported;
    meios::text_outcome out{std::nullopt, meios::eval_failure_kind::none};
    REQUIRE_NOTHROW(out = raising(flaky, reported));
    REQUIRE(out.failure == meios::eval_failure_kind::error);
}
