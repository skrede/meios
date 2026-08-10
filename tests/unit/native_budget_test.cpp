#include "oracle_records.h"

#include "meios/xacro/eval_session.h"
#include "meios/xacro/substitution_detail.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <optional>
#include <algorithm>
#include <functional>
#include <string_view>

namespace
{

struct recorder
{
    std::vector<std::string> messages;

    void operator()(meios::level lvl, const std::string &message)
    {
        if(lvl == meios::level::error)
            messages.push_back(message);
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        (*this)(lvl, message);
    }

    void operator()(meios::level lvl, meios::diagnostic_code, const meios::source_location &,
                    const std::string &message)
    {
        (*this)(lvl, message);
    }
};

struct run
{
    bool survived;
    std::size_t steps;
    std::vector<std::string> messages;
};

run substitute(std::string_view raw, meios::eval_policy policy, meios::detail::eval_session &session)
{
    meios::eval_scope scope;
    meios::source_stack sources;
    recorder heard;
    meios::log_sink_f sink{ std::ref(heard) };
    const meios::expected<meios::substitution, meios::expansion_error> out =
        meios::detail::substitute_refined(raw, scope, sources, "inline.xacro", policy, {}, session,
                                          sink, {});
    return run{ out.has_value(), session.counters.steps, heard.messages };
}

run substitute(std::string_view raw, meios::eval_policy policy, std::size_t step_ceiling)
{
    const meios::evaluator_limits ceilings{ 0, 0, 0, 0, step_ceiling };
    meios::detail::eval_session session(ceilings);
    return substitute(raw, policy, session);
}

run substitute(std::string_view raw, meios::eval_policy policy)
{
    const meios::evaluator_limits defaults;
    meios::detail::eval_session session(defaults);
    return substitute(raw, policy, session);
}

std::string nested(std::size_t depth)
{
    return std::string(depth, '(') + "1" + std::string(depth, ')');
}

std::string prefix_run(std::size_t length)
{
    return std::string(length, '-') + "1";
}

// The shapes are sized from the measurement rather than from the ceiling: each is far past
// the ceiling and, at under five thousand recursive entries, far short of the 38 088 that
// exhaust the main stack these cases run on, so a regression fails the case instead of
// killing the runner. That the byte ceiling is nowhere near reached is what makes the
// refusal the nesting axis's own.
void refuses_on_nesting(const std::string &text)
{
    const run refused = substitute(text, meios::eval_policy::fail);

    CHECK_FALSE(refused.survived);
    REQUIRE(refused.messages.size() == 1);
    CHECK(refused.messages.front().find("expression depth ceiling") != std::string::npos);
    CHECK(text.size() * 100 < meios::default_byte_ceiling);
}

std::size_t longest_measured()
{
    std::size_t longest = 0;
    for(const oracle::row &one : oracle::load_rows("expressions.cases"))
        if(one.fields.size() >= 2)
            longest = std::max(longest, one.fields[1].size());
    return longest;
}

}

TEST_CASE("a deeply nested expression crosses the evaluation-step ceiling", "[native][budget]")
{
    CHECK(substitute("value ${" + nested(8) + "}", meios::eval_policy::fail, 4096).survived);
    CHECK_FALSE(substitute("value ${" + nested(8) + "}", meios::eval_policy::fail, 64).survived);
}

TEST_CASE("a crossed evaluation ceiling is terminal under every evaluation policy",
          "[native][budget]")
{
    for(meios::eval_policy policy :
        { meios::eval_policy::fail, meios::eval_policy::warn, meios::eval_policy::skip })
    {
        CHECK(substitute("value ${1 + 1}", policy, 4096).survived);
        CHECK_FALSE(substitute("value ${1 + 1}", policy, 4).survived);
        CHECK_FALSE(substitute("value ${" + prefix_run(4000) + "}", policy).survived);
        CHECK_FALSE(substitute("value ${" + nested(400) + "}", policy).survived);
    }
}

TEST_CASE("a long run of prefix operators refuses before it reaches the stack",
          "[native][budget]")
{
    refuses_on_nesting("value ${" + prefix_run(4000) + "}");
}

TEST_CASE("a deeply nested expression refuses before it reaches the stack", "[native][budget]")
{
    refuses_on_nesting("value ${" + nested(400) + "}");
}

TEST_CASE("a crossed ceiling reports one diagnostic and stops before the work it bounds",
          "[native][budget]")
{
    const run crossed = substitute("value ${" + nested(8) + "}", meios::eval_policy::fail, 64);

    CHECK_FALSE(crossed.survived);
    CHECK(crossed.messages.size() == 1);
    CHECK(crossed.messages.front().find("evaluation step ceiling") != std::string::npos);
    CHECK(crossed.steps <= 64);
}

TEST_CASE("counters start each load at zero over one shared configuration", "[native][budget]")
{
    const std::string text = "value ${" + nested(8) + "}";
    const meios::evaluator_limits defaults;
    meios::detail::eval_session probe(defaults);
    const std::size_t cost = substitute(text, meios::eval_policy::fail, probe).steps;
    REQUIRE(cost > 0);

    // A ceiling that admits one load's cost and not two, so a counter carried from the first
    // load into the second is exactly what would refuse the second.
    const meios::evaluator_limits shared{ 0, 0, 0, 0, cost + cost / 2 };

    meios::detail::eval_session leaking(shared);
    CHECK(substitute(text, meios::eval_policy::fail, leaking).survived);
    CHECK_FALSE(substitute(text, meios::eval_policy::fail, leaking).survived);

    meios::detail::eval_session first(shared);
    meios::detail::eval_session second(shared);
    CHECK(substitute(text, meios::eval_policy::fail, first).survived);
    CHECK(substitute(text, meios::eval_policy::fail, second).survived);
    CHECK(first.counters.expression_depth > 0);
    CHECK(first.counters.expression_depth == second.counters.expression_depth);
}

// A parenthesis pair is the most expensive spelling per character there is: it re-enters the
// whole precedence descent, and no expression of a given length can nest deeper than half its
// characters. A balanced nest as long as the longest measured expression therefore bounds the
// step cost of every recorded row from above -- at twelve recursive entries per level, which
// is a nesting no recorded row comes near, so the nesting axis is lifted to let the step axis
// be read alone. The nesting headroom over what the rows really contain is asserted apart.
TEST_CASE("the default step ceiling admits the measured expression surface with headroom",
          "[native][budget]")
{
    const std::size_t longest = longest_measured();
    REQUIRE(longest > 0);

    const meios::evaluator_limits lifted{ 0, 0, 0, 0, 0, 6 * longest + 12 };
    meios::detail::eval_session session(lifted);
    const run dominating = substitute("value ${" + nested(longest / 2) + "}",
                                      meios::eval_policy::fail, session);

    CHECK(dominating.survived);
    CHECK(dominating.steps * 10 < lifted.steps);
}
