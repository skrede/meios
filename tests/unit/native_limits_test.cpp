#include "meios/xacro/eval_session.h"

#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <cstddef>

TEST_CASE("no evaluator ceiling can be spelled as disabled", "[native][session]")
{
    const meios::evaluator_limits zeroed{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    REQUIRE(zeroed.yaml_nodes == meios::default_yaml_node_ceiling);
    REQUIRE(zeroed.yaml_depth == meios::default_yaml_depth_ceiling);
    REQUIRE(zeroed.bytes == meios::default_byte_ceiling);
    REQUIRE(zeroed.tokens == meios::default_token_ceiling);
    REQUIRE(zeroed.steps == meios::default_step_ceiling);
    REQUIRE(zeroed.expression_depth == meios::default_expression_depth_ceiling);
    REQUIRE(zeroed.alias_expansion == meios::default_alias_expansion_ceiling);
    REQUIRE(zeroed.numeric_magnitude == meios::default_numeric_magnitude_ceiling);
    REQUIRE(zeroed.expression_tokens == meios::default_expression_token_ceiling);
    REQUIRE(zeroed.string_length == meios::default_string_length_ceiling);

    const meios::evaluator_limits raised{ 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
    REQUIRE(raised.yaml_nodes == 7);
    REQUIRE(raised.steps == 11);
    REQUIRE(raised.expression_depth == 12);
    REQUIRE(raised.alias_expansion == 13);
    REQUIRE(raised.numeric_magnitude == 14);
    REQUIRE(raised.expression_tokens == 15);
    REQUIRE(raised.string_length == 16);
}

// The measured sizes of the pinned acceptance target: 640 auxiliary nodes per load across
// four parse calls, a maximum auxiliary nesting of five, a largest auxiliary document of
// 2 871 bytes, a longest expression of 58 characters, and a deepest expression of 26
// recursive entries -- one bracket level, which is all the target ever nests.
TEST_CASE("the defaults clear the measured acceptance target with headroom", "[native][session]")
{
    const meios::evaluator_limits defaults;

    REQUIRE(defaults.yaml_nodes > 10 * 640);
    REQUIRE(defaults.yaml_depth > 10 * 5);
    REQUIRE(defaults.bytes > 10 * 2871);
    REQUIRE(defaults.tokens > 10 * 58);
    REQUIRE(defaults.steps > 10 * 58);
    REQUIRE(defaults.expression_depth > 9 * 26);
}

// The measured amplification the axis bounds: at a fan-out of nine and one anchor per level,
// five levels denote 672 588 nodes and six denote 6 053 427, while the auxiliary-node ceiling
// sees only the 45 and 54 alias events those two documents raise.
TEST_CASE("the alias ceiling separates the two measured amplification levels", "[native][session]")
{
    constexpr std::size_t denoted_by_five_levels = 672588;
    constexpr std::size_t denoted_by_six_levels = 6053427;

    const meios::evaluator_limits defaults;

    REQUIRE(defaults.alias_expansion >= denoted_by_five_levels);
    REQUIRE(defaults.alias_expansion < denoted_by_six_levels);
    REQUIRE(defaults.yaml_nodes > 54);
}

// Measured by driving a balanced parenthesis nest until the process died, on Linux x86-64
// with an optimized build: 38 088 recursive entries against the default 8192 KiB stack, and
// 2 396 against a 512 KiB thread. The ceiling is sized against the narrow thread, which
// binds it wherever the evaluator runs; re-measuring on another machine is what these two
// numbers are here to invite.
TEST_CASE("the nesting ceiling stays below the depth that exhausts the stack", "[native][session]")
{
    constexpr std::size_t exhausted_on_a_main_stack = 38088;
    constexpr std::size_t exhausted_on_a_small_thread = 2396;

    const meios::evaluator_limits defaults;

    REQUIRE(defaults.expression_depth * 9 < exhausted_on_a_small_thread);
    REQUIRE(defaults.expression_depth * 100 < exhausted_on_a_main_stack);
}

TEST_CASE("a session starts every counter at zero and charges before the work", "[native][session]")
{
    const meios::evaluator_limits ceilings;
    meios::detail::eval_session session(ceilings);
    meios::log_sink silent;

    REQUIRE(session.counters.tokens == 0);
    REQUIRE(session.counters.bytes == 0);
    REQUIRE(session.counters.steps == 0);
    REQUIRE(session.counters.yaml_nodes == 0);
    REQUIRE(session.counters.yaml_depth == 0);
    REQUIRE(session.counters.expression_depth == 0);
    REQUIRE(session.counters.alias_expansion == 0);
    REQUIRE(session.counters.numeric_magnitude == 0);
    REQUIRE(session.counters.expression_tokens == 0);
    REQUIRE(session.counters.string_length == 0);
    REQUIRE(session.counters.constructs.empty());
    REQUIRE(session.failure == meios::eval_failure_kind::none);

    REQUIRE(session.charge_tokens(4, silent, {}));
    REQUIRE(session.counters.tokens == 4);
    REQUIRE(session.admits_yaml_depth(3, silent, {}));
    REQUIRE(session.counters.yaml_depth == 3);
    REQUIRE(session.admits_expression_depth(5, silent, {}));
    REQUIRE(session.counters.expression_depth == 5);
    REQUIRE(session.failure == meios::eval_failure_kind::none);
}
