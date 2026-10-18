#include "native_yaml_probe.h"

#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <string_view>

#ifdef MEIOS_TEST_HAS_YAML

using namespace native_yaml_test;

namespace
{

constexpr std::string_view scalar_leaf = "${xacro.load_yaml(params_file)['a']['b']['c']}";

bool names_a_ceiling(const std::vector<std::string> &messages)
{
    return messages.size() == 1 && messages.front().find("ceiling was reached") != std::string::npos;
}

}

TEST_CASE("a malformed document reports a parse fault rather than a partial value", "[native][yaml]")
{
    const probe out = read("a: *nope\n");

    CHECK_FALSE(out.parsed.has_value());
    CHECK(out.failure == meios::yaml_failure::refused);
    REQUIRE(out.messages.size() == 1);
    CHECK(out.messages.front().find("does not parse") != std::string::npos);
}

TEST_CASE("each auxiliary ceiling is charged before the work it bounds", "[native][yaml]")
{
    const meios::evaluator_limits nodes{2, 0, 0, 0, 0};
    const probe over_nodes = read(nested_document, nodes);
    CHECK_FALSE(over_nodes.parsed.has_value());
    CHECK(over_nodes.failure == meios::yaml_failure::exhausted);
    CHECK(names_a_ceiling(over_nodes.messages));
    CHECK(over_nodes.counters.yaml_nodes <= nodes.yaml_nodes);

    const meios::evaluator_limits depth{0, 2, 0, 0, 0};
    const probe over_depth = read(nested_document, depth);
    CHECK_FALSE(over_depth.parsed.has_value());
    CHECK(over_depth.failure == meios::yaml_failure::exhausted);
    CHECK(names_a_ceiling(over_depth.messages));
    CHECK(over_depth.counters.yaml_depth <= depth.yaml_depth);

    const meios::evaluator_limits bytes{0, 0, 4, 0, 0};
    const probe over_bytes = read(nested_document, bytes);
    CHECK_FALSE(over_bytes.parsed.has_value());
    CHECK(over_bytes.failure == meios::yaml_failure::exhausted);
    CHECK(names_a_ceiling(over_bytes.messages));
    CHECK(over_bytes.counters.bytes == 0);
}

// The reference reaches a scalar leaf, so the same document renders under roomy ceilings and
// the only thing a tight ceiling can be refusing is the ceiling itself.
TEST_CASE("a crossed auxiliary ceiling is terminal under every evaluation policy", "[native][yaml]")
{
    const meios::evaluator_limits ceilings[] = {
            meios::evaluator_limits{2, 0, 0, 0, 0},
            meios::evaluator_limits{0, 2, 0, 0, 0},
            meios::evaluator_limits{0, 0, 4, 0, 0},
    };
    for(const meios::evaluator_limits &crossed : ceilings)
    {
        for(meios::eval_policy policy : {meios::eval_policy::fail, meios::eval_policy::warn, meios::eval_policy::skip})
        {
            const meios::evaluator_limits roomy;
            const run allowed = substitute(nested_document, roomy, policy, scalar_leaf);
            CHECK(allowed.survived);
            CHECK(allowed.text == "value 1");

            const run refused = substitute(nested_document, crossed, policy, scalar_leaf);
            CHECK_FALSE(refused.survived);
            CHECK(names_a_ceiling(refused.messages));
        }
    }
}

// The one distinction leniency turns on: a construct a fuller backend would read is
// unsupported syntax and a lenient policy may retain the span, while a malformed document is
// invalid input and terminates whatever the policy says. The reference backend evaluates a
// tagged value as an expression, so a tagged spelling this evaluator cannot read is the
// unsupported half.
TEST_CASE("a malformed document terminates where an unread construct may be retained", "[native][yaml]")
{
    const meios::evaluator_limits roomy;

    CHECK_FALSE(substitute("a: *nope\n", roomy, meios::eval_policy::skip).survived);

    const run retained = substitute("a: !degrees pi/2\n", roomy, meios::eval_policy::skip);
    CHECK(retained.survived);
    CHECK(retained.text.find("xacro.load_yaml") != std::string::npos);
    CHECK_FALSE(substitute("a: !degrees pi/2\n", roomy, meios::eval_policy::fail).survived);
}

// The other half of the same rule: a construct the reference backend refuses too is terminal,
// so no leniency setting can leave partial output standing where it appeared.
TEST_CASE("a construct the reference backend also refuses stays terminal", "[native][yaml]")
{
    const meios::evaluator_limits roomy;
    const std::string_view terminal[] = {"a: &a [1, *a]\n", "a: !grams 1\n", "? [1]\n: 3\n"};
    for(std::string_view document : terminal)
    {
        INFO("document [" << document << ']');
        for(meios::eval_policy policy : {meios::eval_policy::warn, meios::eval_policy::skip})
            CHECK_FALSE(substitute(document, roomy, policy).survived);
    }
}

#endif
