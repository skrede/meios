#include "eval_python_fixture.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <cstdint>
#include <optional>
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
