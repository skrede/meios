#include "eval_python_refusal_fixture.h"

#include <catch2/catch_test_macros.hpp>

namespace
{

// Only a resolved expansion has a document; asking whether a span survived a refused one
// is asking a question the error arm cannot answer, so the query says so loudly.
bool leaves(const outcome &result, std::string_view span)
{
    REQUIRE(result.expanded);
    return result.expanded->document.find(span) != std::string::npos;
}

}

TEST_CASE("a refusal is still reported under the most lenient policy", "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const outcome refused =
        run(span_document("${__import__('os').getcwd()}"), meios::eval_policy::skip, handle);
    REQUIRE(refused.errors >= 1);
}

TEST_CASE("an expression reaching past arithmetic refuses under a named rule", "[eval_python]")
{
    const meios::eval_scope scope;
    const std::pair<const char *, const char *> refused[] = {
        { "open('/etc/passwd')", "non-allowlisted-builtin" },
        { "getattr(x, '_' + '_class__')", "non-allowlisted-builtin" },
        { "type(1)", "non-allowlisted-builtin" },
        { "vars()", "non-allowlisted-builtin" },
        { "dir()", "non-allowlisted-builtin" },
        { "globals()", "non-allowlisted-builtin" },
        { "eval('1')", "non-allowlisted-builtin" },
        { "exec('1')", "non-allowlisted-builtin" },
        { "compile('1','','eval')", "non-allowlisted-builtin" },
        { "input()", "non-allowlisted-builtin" },
        { "().__class__.__bases__", "dunder-identifier" },
        { "load_yaml.__globals__", "dunder-identifier" },
        { "(lambda: __import__('os').getcwd())()", "dunder-identifier" },
        { "'{0.__globals__}'.format(load_yaml)", "format-traversal" },
        { "'{0.__globals__[builtins]}'.format(load_yaml)", "format-traversal" },
        { "'{0.__self__.__loader__}'.format(sqrt)", "format-traversal" },
        { "'{0.__self__}'.format_map({0: len})", "format-traversal" },
        { "str.format('{0.__self__}', len)", "format-traversal" },
        { "'{}'.format(2)", "format-traversal" },
    };
    for(const std::pair<const char *, const char *> &c : refused)
    {
        INFO(c.first);
        const verdict got = refuse_of(c.first, scope);
        REQUIRE(got.refused);
        REQUIRE(names(got, c.second));
        REQUIRE(names(got, c.first));
    }
}

TEST_CASE("an f-string conversion calling a withheld builtin refuses by that name", "[eval_python]")
{
    meios::eval_scope scope;
    scope.set("x", std::string("arm"));
    scope.set("v", meios::value{ 1.5 });
    REQUIRE(names(refuse_of("f'{x!r}'", scope), "repr"));
    REQUIRE(names(refuse_of("f'{x!a}'", scope), "ascii"));
    const std::optional<std::string> plain = evaluate("f'{x!s}'", scope);
    REQUIRE(plain);
    REQUIRE(*plain == "arm");
    const std::optional<std::string> spec = evaluate("f'{v:.3f}'", scope);
    REQUIRE(spec);
    REQUIRE(*spec == "1.500");
}

TEST_CASE("a dunder inside a string literal is not an identifier", "[eval_python]")
{
    const std::optional<std::string> got =
        evaluate("'a literal containing __import__'", meios::eval_scope{});
    REQUIRE(got);
    REQUIRE(*got == "a literal containing __import__");
}

TEST_CASE("a scope-bound name colliding with a withheld builtin still resolves", "[eval_python]")
{
    meios::eval_scope scope;
    scope.set("type", std::string("revolute"));
    const std::optional<std::string> got = evaluate("'joint_' + type", scope);
    REQUIRE(got);
    REQUIRE(*got == "joint_revolute");
}

TEST_CASE("a scope-bound name spelled format is refused all the same", "[eval_python]")
{
    meios::eval_scope scope;
    scope.set("format", std::string("mesh"));
    REQUIRE(names(refuse_of("format", scope), "format-traversal"));
}

TEST_CASE("the bound yaml helper is compiled, not a prelude function object", "[eval_python]")
{
    const std::optional<std::string> rendered = evaluate("str(load_yaml)", meios::eval_scope{});
    REQUIRE(rendered);
    REQUIRE(rendered->rfind("<function", 0) != 0);
    const std::optional<std::string> shared =
        evaluate("load_yaml is xacro.load_yaml", meios::eval_scope{});
    REQUIRE(shared);
    REQUIRE(*shared == "True");
}

TEST_CASE("the second substitution entry point refuses the same reach", "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const outcome refused =
        run(span_document("$(eval __import__(\"os\"))"), meios::eval_policy::fail, handle);
    REQUIRE_FALSE(refused.expanded.has_value());
    REQUIRE(refused.errors >= 1);
    REQUIRE(refused.expanded.error().message.find("$(eval __import__") == std::string::npos);
}

TEST_CASE("a refusal diagnostic names the expression, the rule and a real position",
          "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const outcome refused =
        run(span_document("${__import__('os').getcwd()}"), meios::eval_policy::fail, handle);
    REQUIRE(says(refused, "__import__"));
    REQUIRE(says(refused, "dunder-identifier"));
    REQUIRE_FALSE(refused.file.empty());
    REQUIRE(refused.line != 0);
}

TEST_CASE("a withheld capability reports the allowlist rule through expand", "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const outcome refused =
        run(span_document("${open('/etc/passwd')}"), meios::eval_policy::fail, handle);
    REQUIRE_FALSE(refused.expanded.has_value());
    REQUIRE(says(refused, "non-allowlisted-builtin"));
}

TEST_CASE("the formatting traversal vector reports its rule and leaks nothing", "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const outcome refused = run(span_document("${'{0.__globals__}'.format(load_yaml)}"),
                                meios::eval_policy::fail, handle);
    REQUIRE_FALSE(refused.expanded.has_value());
    REQUIRE(says(refused, "format-traversal"));
    REQUIRE(refused.expanded.error().message.find("parse_yaml") == std::string::npos);
    REQUIRE(refused.expanded.error().message.find("allowed_builtins") == std::string::npos);
}

TEST_CASE("a dunder inside a string literal still reaches the expanded document", "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const outcome resolved = run(span_document("${'a literal containing __import__'}"),
                                 meios::eval_policy::fail, handle);
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(leaves(resolved, "<l>a literal containing __import__</l>"));
}

TEST_CASE("the allowlisted names a description actually uses still evaluate", "[eval_python]")
{
    const meios::eval_scope scope;
    const std::pair<const char *, const char *> allowed[] = {
        { "sqrt(2)", "1.4142135623730951" }, { "radians(180)", "3.141592653589793" },
        { "min(1,2)", "1" },                 { "abs(-3.5)", "3.5" },
        { "2**64", "18446744073709551616" }, { "len(sorted([3,1,2]))", "3" },
        { "sum(range(4))", "6" },            { "pow(2,3)", "8.0" },
    };
    for(const std::pair<const char *, const char *> &c : allowed)
    {
        INFO(c.first);
        const std::optional<std::string> got = evaluate(c.first, scope);
        REQUIRE(got);
        REQUIRE(*got == c.second);
    }
}