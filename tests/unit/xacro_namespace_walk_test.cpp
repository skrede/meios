#include "stack_meter.h"
#include "xacro_include_tree.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/capturing_log_sink.h>
#include <meios/diagnostic/captured_diagnostic.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>

namespace
{

constexpr std::size_t thread_stack = 512 * 1024;
// The default ceiling of 188 less the thirteen levels a dotted read already holds and the one its
// last member takes.
constexpr std::size_t longest_admitted = 174;

struct chain_read
{
    std::optional<std::string> document;
    meios::diagnostic_code code;
    std::string message;
    std::size_t errors;
    std::string first_record;
};

std::string hop_chain(std::size_t hops)
{
    std::string expression = "${n";
    for(std::size_t hop = 0; hop < hops; ++hop)
        expression += ".n";
    return expression + ".x}";
}

chain_read read_document(const include_tree::tree &held, std::string_view included, std::string_view body)
{
    held.write("w.xacro", include_tree::robot(included));
    const std::filesystem::path document = held.write("top.xacro", include_tree::robot(body));
    meios::log_sink silent;
    meios::capturing_log_sink log(silent);
    meios::source_stack sources{};
    meios::eval_scope scope;
    const include_tree::expansion_result out              = meios::expand(include_tree::read_file(document), scope, sources, document, meios::expansion_limits{}, log);
    const std::optional<meios::captured_diagnostic> first = log.first();
    const std::string record                              = first ? first->message : std::string();
    if(out.has_value())
        return chain_read{out->document, meios::diagnostic_code::unspecified, std::string(), log.errors(), record};
    return chain_read{std::nullopt, out.error().code, out.error().message, log.errors(), record};
}

chain_read read_expression(const include_tree::tree &held, std::string_view expression)
{
    const std::string body = "<xacro:include filename=\"w.xacro\" ns=\"n\"/><link name=\"l" + std::string(expression) + "\"/>";
    return read_document(held, "<xacro:property name=\"x\" value=\"7\"/>", body);
}

// The included file binds only c, so every other member a read names falls through past it.
chain_read read_through(const include_tree::tree &held, std::string_view ns, std::string_view expression, std::string_view before = {})
{
    const std::string body = std::string(before) + "<xacro:include filename=\"w.xacro\" ns=\"" + std::string(ns) + "\"/><link name=\"l" + std::string(expression) + "\"/>";
    return read_document(held, "<xacro:property name=\"c\" value=\"C\"/>", body);
}

bool refuses_undefined(const chain_read &got, std::string_view name)
{
    return !got.document && got.code == meios::diagnostic_code::undefined_property && got.message.find("name '" + std::string(name) + "' is not defined") != std::string::npos;
}

chain_read read_chain(const include_tree::tree &held, std::size_t hops)
{
    return read_expression(held, hop_chain(hops));
}

bool renders(const chain_read &got, std::string_view name)
{
    return got.document && got.document->find("name=\"" + std::string(name) + '"') != std::string::npos;
}

bool refuses_on_depth(const chain_read &got)
{
    return !got.document && got.code == meios::diagnostic_code::expansion_budget_exceeded && got.message.find("expression depth ceiling") != std::string::npos && got.errors == 1 &&
            got.first_record.find("expression depth ceiling") != std::string::npos;
}

std::size_t longest_renderable(const include_tree::tree &held, std::size_t cap)
{
    std::size_t hops = 0;
    while(hops < cap && renders(read_chain(held, hops + 1), "l7"))
        ++hops;
    return hops;
}

}

TEST_CASE("a dotted read thousands of namespaces deep refuses on the expression depth ceiling", "[xacro][namespace]")
{
    const include_tree::tree held;
    std::optional<chain_read> got;
    REQUIRE(stack_meter::run_on_stack(thread_stack, [&] { got = read_chain(held, 4000); }));
    REQUIRE(got);
    CHECK(refuses_on_depth(*got));
}

TEST_CASE("the longest dotted read through namespaces renders and one hop more refuses", "[xacro][namespace]")
{
    const include_tree::tree held;
    std::size_t longest = 0;
    std::optional<chain_read> past;
    REQUIRE(stack_meter::run_on_stack(thread_stack,
                                      [&]
                                      {
                                          longest = longest_renderable(held, longest_admitted + 1);
                                          past    = read_chain(held, longest + 1);
                                      }));
    CHECK(longest == 174);
    REQUIRE(past);
    CHECK(refuses_on_depth(*past));
}

TEST_CASE("a dotted read through namespaces holds the same stack at every length", "[xacro][namespace]")
{
    const include_tree::tree held;
    // A first read initializes statics whose frames would otherwise count against the one-hop figure alone.
    read_chain(held, 1);
    const std::optional<std::size_t> one     = stack_meter::stack_used(thread_stack, [&] { read_chain(held, 1); });
    const std::optional<std::size_t> longest = stack_meter::stack_used(thread_stack, [&] { read_chain(held, longest_admitted); });
    if(!one || !longest)
        SKIP("this platform offers no stack measurement");
    CHECK(*longest < *one + 4096);
}

TEST_CASE("a dotted read through one or two namespaces renders the member", "[xacro][namespace]")
{
    const include_tree::tree held;
    CHECK(renders(read_expression(held, "${n.x}"), "l7"));
    CHECK(renders(read_expression(held, "${n.n.x}"), "l7"));
}

TEST_CASE("a dotted read stopping at a nested namespace refuses naming its spelling", "[xacro][namespace]")
{
    const include_tree::tree held;
    const chain_read got = read_expression(held, "${n.n}");
    CHECK(!got.document);
    CHECK(got.code == meios::diagnostic_code::expression_error);
    CHECK(got.message.find("'n.n' names an include namespace") != std::string::npos);
}

TEST_CASE("a read through an include namespace falls through to the mathematics constant", "[xacro][namespace]")
{
    const include_tree::tree held;
    CHECK(renders(read_through(held, "n", "${n.pi}"), "l3.141592653589793"));
    CHECK(renders(read_through(held, "math", "${math.pi}"), "l3.141592653589793"));
    CHECK(renders(read_through(held, "n", "${n.n.pi}"), "l3.141592653589793"));
}

TEST_CASE("a call through an include namespace dispatches as the bare call does", "[xacro][namespace]")
{
    const include_tree::tree held;
    CHECK(renders(read_through(held, "n", "${n.sin(0)}"), "l0.0"));
    CHECK(renders(read_through(held, "n", "${n.max(1, 2)}"), "l2"));
}

TEST_CASE("a call to a function the global table does not list refuses through an include namespace", "[xacro][namespace]")
{
    const include_tree::tree held;
    CHECK(refuses_undefined(read_through(held, "n", "${n.abs(-2)}"), "abs"));
}

TEST_CASE("a global name other than pi refuses through an include namespace under the built-in evaluator, each constant as it refuses bare, where the reference renders it",
          "[xacro][namespace]")
{
    const include_tree::tree held;
    for(const char *constant : {"e", "tau", "inf", "nan"})
    {
        INFO(constant);
        CHECK(refuses_undefined(read_through(held, "n", std::string("${n.") + constant + "}"), constant));
        CHECK(refuses_undefined(read_through(held, "n", std::string("${") + constant + "}"), constant));
    }
    CHECK(refuses_undefined(read_through(held, "n", "${n.n.e}"), "e"));
    CHECK(refuses_undefined(read_through(held, "n", "${n.math.pi}"), "math"));
}

TEST_CASE("a property of the including file wins over the mathematics constant", "[xacro][namespace]")
{
    const include_tree::tree held;
    CHECK(renders(read_through(held, "n", "${n.pi}", "<xacro:property name=\"pi\" value=\"3\"/>"), "l3"));
}
