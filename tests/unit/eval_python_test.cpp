#include "eval_python_fixture.h"

#include <catch2/catch_test_macros.hpp>

namespace
{

struct golden_case
{
    std::string expr;
    std::string expected;
};

std::vector<golden_case> load_cases(const std::string &name)
{
    const std::filesystem::path path = std::filesystem::path{ MEIOS_GOLDEN_DIR } / "expr" / name;
    std::ifstream in(path);
    std::vector<golden_case> cases;
    std::string line;
    while(std::getline(in, line))
    {
        std::size_t tab = line.find('\t');
        if(line.empty() || line[0] == '#' || tab == std::string::npos)
            continue;
        cases.push_back({ line.substr(0, tab), line.substr(tab + 1) });
    }
    return cases;
}

meios::eval_scope serving(std::string text)
{
    meios::eval_scope scope;
    install_yaml(scope, std::move(text));
    return scope;
}

}

static_assert(meios::text_evaluator<meios::python_evaluator>);

TEST_CASE("python evaluator matches the golden CPython corpus", "[eval_python]")
{
    const meios::eval_scope scope;
    for(const char *file : { "arithmetic.cases", "functions.cases", "precedence.cases" })
    {
        for(const golden_case &c : load_cases(file))
        {
            INFO(file << ": " << c.expr);
            const std::optional<std::string> got = evaluate(c.expr, scope);
            REQUIRE(got);
            REQUIRE(*got == c.expected);
        }
    }
}

TEST_CASE("a mathematics name is bound bare and carries no math namespace", "[eval_python]")
{
    const meios::eval_scope scope;
    const std::optional<std::string> bare = evaluate("sqrt(2)", scope);
    REQUIRE(bare);
    REQUIRE(*bare == "1.4142135623730951");

    tally counts;
    meios::log_sink_f sink{ std::ref(counts) };
    meios::python_evaluator evaluator;
    REQUIRE_FALSE(evaluator.eval_to_text("math.sqrt(2)", scope, sink));
    REQUIRE(evaluator.last_failure_kind() == meios::eval_failure_kind::error);
    REQUIRE(counts.last.find("NameError") != std::string::npos);
}

TEST_CASE("a string-producing expression returns its python str", "[eval_python]")
{
    meios::eval_scope scope;
    scope.set("name", std::string("arm"));
    const std::optional<std::string> got = evaluate("'link_' + name", scope);
    REQUIRE(got);
    REQUIRE(*got == "link_arm");
}

TEST_CASE("a comprehension evaluates with full python parity", "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const outcome resolved = run(span_document("${[i*i for i in range(4)]}"),
                                 meios::eval_policy::fail, handle);
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(leaves(resolved, "<l>[0, 1, 4, 9]</l>"));
}

TEST_CASE("an arbitrary-precision integer renders its exact CPython decimal", "[eval_python]")
{
    const std::optional<std::string> got = evaluate("2**64", meios::eval_scope{});
    REQUIRE(got);
    REQUIRE(*got == "18446744073709551616");
}

TEST_CASE("a seeded numeric property matches core formatting", "[eval_python]")
{
    meios::eval_scope scope;
    scope.set("dof", meios::value{ static_cast<long long>(6) });
    const std::optional<std::string> got = evaluate("dof * 2", scope);
    REQUIRE(got);
    REQUIRE(*got == "12");
}

TEST_CASE("load_yaml wraps yaml.safe_load over a found interpreter", "[eval_python]")
{
    const meios::eval_scope scope = serving("arm:\n  dof: 6\n");
    const std::optional<std::string> got = evaluate("load_yaml('config.yaml')['arm']['dof']", scope);
    REQUIRE(got);
    REQUIRE(*got == "6");
}

TEST_CASE("an injected python backend resolves a document construct", "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const outcome resolved = run(span_document("${[i*i for i in range(3)]}"),
                                 meios::eval_policy::fail, handle);
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(leaves(resolved, "[0, 1, 4]"));
}

TEST_CASE("a non-python construct resolves identically with the backend injected", "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const outcome resolved = run(span_document("${1+1}"), meios::eval_policy::fail, handle);
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(leaves(resolved, "<l>2</l>"));
}

TEST_CASE("a python runtime throw hard-fails even under skip", "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const outcome raised = run(span_document("${1/0}"), meios::eval_policy::skip, handle);
    REQUIRE_FALSE(raised.expanded.has_value());
    REQUIRE(raised.errors >= 1);
    REQUIRE(raised.expanded.error().message.find("${1/0}") == std::string::npos);
}

TEST_CASE("xacro.load_yaml resolves a mapping through the SimpleNamespace shim", "[eval_python]")
{
    const meios::eval_scope scope = serving("arm:\n  dof: 6\n");
    const std::optional<std::string> got =
        evaluate("xacro.load_yaml('config.yaml')['arm']['dof']", scope);
    REQUIRE(got);
    REQUIRE(*got == "6");
}

TEST_CASE("a !degrees yaml tag loads as radians through the SafeLoader constructor", "[eval_python]")
{
    const meios::eval_scope scope = serving("angle: !degrees 180\n");
    const std::optional<std::string> got =
        evaluate("abs(xacro.load_yaml('a.yaml')['angle'] - pi) < 1e-9", scope);
    REQUIRE(got);
    REQUIRE(*got == "True");
}

TEST_CASE("a !radians yaml tag loads unchanged through the SafeLoader constructor", "[eval_python]")
{
    const meios::eval_scope scope = serving("angle: !radians 1.5\n");
    const std::optional<std::string> got =
        evaluate("abs(xacro.load_yaml('a.yaml')['angle'] - 1.5) < 1e-9", scope);
    REQUIRE(got);
    REQUIRE(*got == "True");
}
TEST_CASE("an import-reaching expression is refused under every evaluation policy", "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    for(meios::eval_policy policy :
        { meios::eval_policy::fail, meios::eval_policy::warn, meios::eval_policy::skip })
    {
        INFO("policy " << static_cast<int>(policy));
        const outcome refused = run(span_document("${__import__('os').getcwd()}"), policy, handle);
        REQUIRE_FALSE(refused.expanded.has_value());
        REQUIRE(refused.errors >= 1);
        REQUIRE(refused.expanded.error().message.find("${__import__") == std::string::npos);
    }
}
