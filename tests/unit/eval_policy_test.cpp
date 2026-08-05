#include "eval_policy_fixture.h"

#include <catch2/catch_test_macros.hpp>

static_assert(meios::text_evaluator<fixed_backend>);
static_assert(meios::text_evaluator<declining_backend>);
static_assert(!std::is_copy_constructible_v<meios::evaluator_handle>);
static_assert(std::is_move_constructible_v<meios::evaluator_handle>);

TEST_CASE("a supported expression resolves under every policy", "[xacro][eval_policy]")
{
    const outcome resolved = run(span_document("${1+1}"), meios::eval_policy::fail);
    REQUIRE(resolved.ok);
    REQUIRE(leaves(resolved, "2"));
    REQUIRE(resolved.errors == 0);
}

TEST_CASE("fail policy aborts on an unsupported construct with a diagnostic", "[xacro][eval_policy]")
{
    const outcome aborted = run(span_document("${gaussian(1)}"), meios::eval_policy::fail);
    REQUIRE_FALSE(aborted.ok);
    REQUIRE(aborted.errors >= 1);
}

TEST_CASE("warn policy leaves an unsupported span verbatim and logs", "[xacro][eval_policy]")
{
    const outcome lenient = run(span_document("${gaussian(1)}"), meios::eval_policy::warn);
    REQUIRE(lenient.ok);
    REQUIRE(leaves(lenient, "${gaussian(1)}"));
    REQUIRE(lenient.warnings >= 1);
    REQUIRE(lenient.errors == 0);
}

TEST_CASE("skip policy leaves an unsupported span verbatim and quietly", "[xacro][eval_policy]")
{
    const outcome quiet = run(span_document("${gaussian(1)}"), meios::eval_policy::skip);
    REQUIRE(quiet.ok);
    REQUIRE(leaves(quiet, "${gaussian(1)}"));
    REQUIRE(quiet.warnings == 0);
    REQUIRE(quiet.errors == 0);
}

TEST_CASE("a genuine evaluation error hard-fails even under skip", "[xacro][eval_policy]")
{
    const outcome undefined = run(span_document("${undefined_property}"), meios::eval_policy::skip);
    REQUIRE_FALSE(undefined.ok);
    REQUIRE(undefined.errors >= 1);
    REQUIRE_FALSE(leaves(undefined, "${undefined_property}"));
}

TEST_CASE("an unsupported conditional hard-fails even under skip", "[xacro][eval_policy]")
{
    const outcome structural = run(conditional_document("${gaussian(1)}"), meios::eval_policy::skip);
    REQUIRE_FALSE(structural.ok);
    REQUIRE(structural.errors >= 1);
}

TEST_CASE("an injected backend is consulted ahead of the core", "[xacro][eval_policy]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(fixed_backend{});
    const outcome injected = run(span_document("${anything}"), meios::eval_policy::fail, handle);
    REQUIRE(injected.ok);
    REQUIRE(leaves(injected, "7"));
}

TEST_CASE("an injected unsupported decline is left verbatim under skip", "[xacro][eval_policy]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(declining_backend{});
    const outcome declined = run(span_document("${anything}"), meios::eval_policy::skip, handle);
    REQUIRE(declined.ok);
    REQUIRE(leaves(declined, "${anything}"));
}

TEST_CASE("an injected genuine error hard-fails regardless of policy", "[xacro][eval_policy]")
{
    const declining_backend erroring{ meios::eval_failure_kind::error };
    const auto handle = std::make_shared<meios::evaluator_handle>(erroring);
    const outcome failed = run(span_document("${anything}"), meios::eval_policy::skip, handle);
    REQUIRE_FALSE(failed.ok);
}

TEST_CASE("a leading python-only span is left verbatim under skip and warn", "[xacro][eval_policy]")
{
    const outcome skipped = run(span_document("${['x']}"), meios::eval_policy::skip);
    REQUIRE(skipped.ok);
    REQUIRE(leaves(skipped, "${['x']}"));
    REQUIRE(skipped.errors == 0);

    const outcome warned = run(span_document("${['x']}"), meios::eval_policy::warn);
    REQUIRE(warned.ok);
    REQUIRE(leaves(warned, "${['x']}"));
    REQUIRE(warned.errors == 0);
    REQUIRE(warned.warnings >= 1);
}

TEST_CASE("a leading python-only span still aborts under fail", "[xacro][eval_policy]")
{
    const outcome aborted = run(span_document("${['x']}"), meios::eval_policy::fail);
    REQUIRE_FALSE(aborted.ok);
    REQUIRE(aborted.errors >= 1);
}

TEST_CASE("a malformed numeric literal hard-fails even under skip", "[xacro][eval_policy]")
{
    const outcome broken = run(span_document("${1.2.3}"), meios::eval_policy::skip);
    REQUIRE_FALSE(broken.ok);
    REQUIRE(broken.errors >= 1);
    REQUIRE_FALSE(leaves(broken, "${1.2.3}"));
}
