#include <meios/xacro.h>

#include <rapidcheck.h>
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstdint>

namespace
{

struct arith_form
{
    std::string text;
    std::int64_t value;
};

struct bool_form
{
    std::string text;
    bool value;
};

bool_form boolean(std::int32_t depth);

// rapidcheck narrows a range by the current size, which would leave the deeper shapes rare in the
// small early cases; the shape is drawn at full size so every production is reached throughout.
std::int32_t shape_at(std::int32_t depth)
{
    return depth == 0 ? 0 : *rc::gen::resize(100, rc::gen::inRange<std::int32_t>(0, 5));
}

// Eight leaves of at most nine keep every product far under the numeric-magnitude ceiling.
arith_form arithmetic(std::int32_t depth)
{
    const std::int32_t shape = shape_at(depth);
    if(shape == 0)
    {
        const std::int64_t leaf = *rc::gen::inRange<std::int64_t>(-9, 10);
        return {std::to_string(leaf), leaf};
    }
    const arith_form left  = arithmetic(depth - 1);
    const arith_form right = arithmetic(depth - 1);
    if(shape == 1)
        return {"(" + left.text + " + " + right.text + ")", left.value + right.value};
    if(shape == 2)
        return {"(" + left.text + " - " + right.text + ")", left.value - right.value};
    if(shape == 3)
        return {"(" + left.text + " * " + right.text + ")", left.value * right.value};
    const bool_form condition = boolean(depth - 1);
    return {"(" + left.text + " if " + condition.text + " else " + right.text + ")", condition.value ? left.value : right.value};
}

bool_form boolean(std::int32_t depth)
{
    const std::int32_t shape = shape_at(depth);
    if(shape == 0)
    {
        const bool leaf = *rc::gen::arbitrary<bool>();
        return {leaf ? "True" : "False", leaf};
    }
    if(shape == 1)
    {
        const bool_form inner = boolean(depth - 1);
        return {"(not " + inner.text + ")", !inner.value};
    }
    if(shape == 2)
    {
        const arith_form left  = arithmetic(depth - 1);
        const arith_form right = arithmetic(depth - 1);
        return {"(" + left.text + " < " + right.text + ")", left.value < right.value};
    }
    const bool_form left  = boolean(depth - 1);
    const bool_form right = boolean(depth - 1);
    if(shape == 3)
        return {"(" + left.text + " and " + right.text + ")", left.value && right.value};
    return {"(" + left.text + " or " + right.text + ")", left.value || right.value};
}

std::string evaluated(const std::string &expression)
{
    meios::core_evaluator evaluator;
    const meios::eval_scope scope;
    meios::log_sink sink;
    const meios::value result = evaluator.eval(expression, scope, sink);
    RC_ASSERT_FALSE(evaluator.failed());
    return meios::render_scalar(result).value_or(std::string{});
}

std::int32_t some_depth()
{
    return *rc::gen::resize(100, rc::gen::inRange<std::int32_t>(0, 4));
}

}

TEST_CASE("well-formed arithmetic, boolean and conditional forms evaluate to their reference", "[native][property]")
{
    REQUIRE(rc::check("an arithmetic form equals its reference evaluation",
                      []
                      {
                          const arith_form form = arithmetic(some_depth());
                          RC_TAG(form.text.find(" if ") == std::string::npos ? "arithmetic only" : "with a conditional");
                          RC_ASSERT(evaluated(form.text) == std::to_string(form.value));
                      }));

    REQUIRE(rc::check("a boolean form equals its reference evaluation",
                      []
                      {
                          const bool_form form = boolean(some_depth());
                          RC_TAG(form.text.find(" and ") != std::string::npos || form.text.find(" or ") != std::string::npos ? "with a connective" : "without a connective");
                          RC_ASSERT(evaluated(form.text) == (form.value ? "True" : "False"));
                      }));
}
