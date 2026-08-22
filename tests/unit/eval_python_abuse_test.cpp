#include "abuse_probe.h"
#include "abuse_table.h"

#include <meios/eval/python_evaluator.h>
#include <meios/eval/unrestricted_python_evaluator.h>

#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <optional>

namespace
{

void refuses_through_expansion(const std::string &span, meios::eval_policy policy)
{
    const abuse::outcome refused = abuse::expand_with_python(abuse::span_document(span), policy);
    REQUIRE_FALSE(refused.expanded.has_value());
    REQUIRE(refused.messages.size() >= 1);
    REQUIRE(refused.expanded.error().message.find(span) == std::string::npos);
}

}

TEST_CASE("the table's rule column is exactly the vocabulary the evaluator emits", "[eval_python]")
{
    const std::vector<abuse::row> rows = abuse::load_rows();
    REQUIRE(rows.size() >= 38);
    for(const abuse::row &r : rows)
    {
        INFO(r.expr);
        if(r.verdict == "refuse")
            REQUIRE(abuse::emitted(r.rule));
        else
        {
            REQUIRE(r.verdict == "allow");
            REQUIRE(r.rule == "-");
        }
    }
}

TEST_CASE("every refusing row refuses through the direct seam under its own rule", "[eval_python]")
{
    const meios::eval_scope scope = abuse::seeded();
    for(const abuse::row &r : abuse::load_rows())
    {
        if(r.verdict != "refuse")
            continue;
        INFO(r.expr);
        const abuse::verdict got = abuse::refuse_of<meios::python_evaluator>(r.expr, scope);
        REQUIRE(got.refused);
        REQUIRE(abuse::names(got, r.rule));
    }
}

TEST_CASE("every refusing row refuses through a full expansion under either policy", "[eval_python]")
{
    const meios::eval_scope scope = abuse::seeded();
    for(const abuse::row &r : abuse::load_rows())
    {
        if(r.verdict != "refuse" || abuse::answered_by_scope(r, scope))
            continue;
        INFO(r.expr);
        const std::string span = "${" + r.expr + "}";
        refuses_through_expansion(span, meios::eval_policy::fail);
        refuses_through_expansion(span, meios::eval_policy::skip);
    }
}

TEST_CASE("a lone identifier bound to a string is answered before the evaluator sees it",
          "[eval_python]")
{
    const abuse::outcome resolved =
        abuse::expand_with_python(abuse::span_document("${format}"), meios::eval_policy::fail);
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(resolved.expanded->document.find("<l>stl</l>") != std::string::npos);
    REQUIRE(resolved.messages.empty());
}

TEST_CASE("every allowing row evaluates through the restricted evaluator", "[eval_python]")
{
    const meios::eval_scope scope = abuse::seeded();
    for(const abuse::row &r : abuse::load_rows())
    {
        if(r.verdict != "allow")
            continue;
        INFO(r.expr);
        REQUIRE(abuse::evaluate<meios::python_evaluator>(r.expr, scope));
    }
}

TEST_CASE("a bound property spelled for the formatting method still refuses by that rule",
          "[eval_python]")
{
    const meios::eval_scope scope = abuse::seeded();
    REQUIRE(scope.lookup("format"));
    const abuse::verdict got = abuse::refuse_of<meios::python_evaluator>("format", scope);
    REQUIRE(got.refused);
    REQUIRE(abuse::names(got, "format-traversal"));
}

TEST_CASE("an uncontained resource row refuses under either evaluator class", "[eval_python]")
{
    const meios::eval_scope scope = abuse::seeded();
    for(const abuse::row &r : abuse::load_rows())
    {
        if(r.rule != "uncontained-yaml-path")
            continue;
        INFO(r.expr);
        const abuse::verdict got =
            abuse::refuse_of<meios::unrestricted_python_evaluator>(r.expr, scope);
        REQUIRE(got.refused);
        REQUIRE(abuse::names(got, r.rule));
    }
}

TEST_CASE("a resolving yaml row renders identically under either evaluator class", "[eval_python]")
{
    const meios::eval_scope scope = abuse::seeded();
    for(const abuse::row &r : abuse::load_rows())
    {
        if(!abuse::resolving_yaml(r))
            continue;
        INFO(r.expr);
        const std::optional<std::string> got = abuse::evaluate<meios::python_evaluator>(r.expr, scope);
        REQUIRE(got);
        REQUIRE(got == abuse::evaluate<meios::unrestricted_python_evaluator>(r.expr, scope));
    }
}
