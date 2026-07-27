#include <meios/eval/python_evaluator.h>

#include <meios/xacro.h>
#include <meios/io/source_stack.h>

#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>

namespace
{

struct tally
{
    int errors{ 0 };
    int line{ 0 };
    std::string file;
    std::vector<std::string> messages;

    void record(meios::level lvl, const std::string &message)
    {
        if(lvl != meios::level::error)
            return;
        ++errors;
        messages.push_back(message);
    }

    void operator()(meios::level lvl, const std::string &message)
    {
        record(lvl, message);
    }

    void operator()(meios::level lvl, const meios::source_location &where,
                    const std::string &message)
    {
        record(lvl, message);
        anchor(lvl, where);
    }

    void operator()(meios::level lvl, meios::diagnostic_code, const meios::source_location &where,
                    const std::string &message)
    {
        record(lvl, message);
        anchor(lvl, where);
    }

    void anchor(meios::level lvl, const meios::source_location &where)
    {
        if(lvl != meios::level::error)
            return;
        file = where.file.string();
        line = where.line;
    }
};

std::optional<std::string> evaluate(std::string_view expr, const meios::eval_scope &scope)
{
    meios::python_evaluator evaluator;
    meios::log_sink silent;
    return evaluator.eval_to_text(expr, scope, silent);
}

struct verdict
{
    bool refused;
    std::vector<std::string> messages;
};

verdict refuse_of(std::string_view expr, const meios::eval_scope &scope)
{
    tally counts;
    meios::log_sink_f sink{ std::ref(counts) };
    meios::python_evaluator evaluator;
    const bool declined = !evaluator.eval_to_text(expr, scope, sink)
        && evaluator.last_failure_kind() == meios::eval_failure_kind::refused;
    return { declined, std::move(counts.messages) };
}

// A resource refusal reports twice — the fetch names the spec, the classifier names the rule
// — so a fragment is looked for across the whole record rather than in the first line.
bool names(const verdict &got, std::string_view fragment)
{
    for(const std::string &message : got.messages)
    {
        if(message.find(fragment) != std::string::npos)
            return true;
    }
    return false;
}

struct fixed_text final : meios::text_resource_loader::fetcher
{
    explicit fixed_text(std::optional<std::string> text) : m_text(std::move(text)) {}

    std::optional<std::string> fetch(std::string_view, const std::filesystem::path &) override
    {
        return m_text;
    }

    std::optional<std::string> m_text;
};

meios::eval_scope serving(std::optional<std::string> text)
{
    meios::eval_scope scope;
    scope.install_text_loader(meios::text_resource_loader{ std::make_unique<fixed_text>(std::move(text)) });
    return scope;
}

struct outcome
{
    bool ok;
    int errors;
    int line;
    std::string file;
    std::string document;
    std::vector<std::string> messages;
};

outcome run(std::string_view source, meios::eval_policy policy,
            const std::shared_ptr<meios::evaluator_handle> &backend)
{
    tally counts;
    meios::log_sink_f sink{ std::ref(counts) };
    meios::eval_scope scope;
    meios::source_stack sources;
    meios::expansion out = meios::expand(source, scope, sources, "robot.xacro",
                                         meios::expansion_limits{}, policy, backend, sink);
    return outcome{ out.ok,      counts.errors,           counts.line,
                    counts.file, std::move(out.document), std::move(counts.messages) };
}

bool says(const outcome &result, std::string_view fragment)
{
    for(const std::string &message : result.messages)
    {
        if(message.find(fragment) != std::string::npos)
            return true;
    }
    return false;
}

constexpr std::string_view header = "<robot xmlns:xacro=\"http://ros.org/wiki/xacro\">";

std::string span_document(std::string_view inner)
{
    return std::string(header) + "<l>" + std::string(inner) + "</l></robot>";
}

bool leaves(const outcome &result, std::string_view span)
{
    return result.document.find(span) != std::string::npos;
}

}

TEST_CASE("the yaml helper parses the text the scope's loader hands it", "[eval_python]")
{
    const meios::eval_scope scope = serving("arm:\n  dof: 6\n");
    const std::optional<std::string> bare = evaluate("load_yaml('config.yaml')['arm']['dof']", scope);
    REQUIRE(bare);
    REQUIRE(*bare == "6");
    const std::optional<std::string> shim =
        evaluate("xacro.load_yaml('config.yaml')['arm']['dof']", scope);
    REQUIRE(shim);
    REQUIRE(*shim == "6");
}

TEST_CASE("a scope carrying no loader refuses a yaml request rather than reading a file",
          "[eval_python]")
{
    const meios::eval_scope bare;
    const verdict got = refuse_of("load_yaml('/etc/passwd')", bare);
    REQUIRE(got.refused);
    REQUIRE(names(got, "uncontained-yaml-path"));
    REQUIRE(names(got, "/etc/passwd"));
}

TEST_CASE("a loader that refuses a spec reports the refusal kind and names the rule",
          "[eval_python]")
{
    const meios::eval_scope scope = serving(std::nullopt);
    const verdict got = refuse_of("load_yaml('../../../etc/passwd')", scope);
    REQUIRE(got.refused);
    REQUIRE(names(got, "uncontained-yaml-path"));
    REQUIRE(names(got, "../../../etc/passwd"));
}

TEST_CASE("an uncontained yaml spec refuses through expand and leaks no file content",
          "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const outcome refused =
        run(span_document("${load_yaml('/etc/passwd')}"), meios::eval_policy::skip, handle);
    REQUIRE_FALSE(refused.ok);
    REQUIRE(refused.errors >= 1);
    REQUIRE(says(refused, "uncontained-yaml-path"));
    REQUIRE_FALSE(leaves(refused, "root:"));
    REQUIRE_FALSE(leaves(refused, "${load_yaml"));
}

TEST_CASE("every unit tag the reference implementation registers constructs", "[eval_python]")
{
    const meios::eval_scope scope = serving("a: !radians 1.5\n"
                                            "b: !degrees 180\n"
                                            "c: !meters 2.0\n"
                                            "d: !millimeters 1000\n"
                                            "e: !foot 1\n"
                                            "f: !inches 1\n");
    const std::pair<const char *, const char *> tagged[] = {
        { "abs(load_yaml('u')['a'] - 1.5) < 1e-9", "True" },
        { "abs(load_yaml('u')['b'] - pi) < 1e-9", "True" },
        { "abs(load_yaml('u')['c'] - 2.0) < 1e-9", "True" },
        { "abs(load_yaml('u')['d'] - 1.0) < 1e-9", "True" },
        { "abs(load_yaml('u')['e'] - 0.3048) < 1e-9", "True" },
        { "abs(load_yaml('u')['f'] - 0.0254) < 1e-9", "True" },
    };
    for(const std::pair<const char *, const char *> &c : tagged)
    {
        INFO(c.first);
        const std::optional<std::string> got = evaluate(c.first, scope);
        REQUIRE(got);
        REQUIRE(*got == c.second);
    }
}

TEST_CASE("a yaml text without a trailing newline and an empty one both parse", "[eval_python]")
{
    const meios::eval_scope clipped = serving("arm:\n  dof: 6");
    const std::optional<std::string> got = evaluate("load_yaml('c')['arm']['dof']", clipped);
    REQUIRE(got);
    REQUIRE(*got == "6");
    const meios::eval_scope empty = serving(std::string{});
    const std::optional<std::string> none = evaluate("str(load_yaml('c'))", empty);
    REQUIRE(none);
    REQUIRE(*none == "None");
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
    REQUIRE_FALSE(refused.ok);
    REQUIRE(refused.errors >= 1);
    REQUIRE_FALSE(leaves(refused, "$(eval __import__"));
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
    REQUIRE_FALSE(refused.ok);
    REQUIRE(says(refused, "non-allowlisted-builtin"));
}

TEST_CASE("the formatting traversal vector reports its rule and leaks nothing", "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const outcome refused = run(span_document("${'{0.__globals__}'.format(load_yaml)}"),
                                meios::eval_policy::fail, handle);
    REQUIRE_FALSE(refused.ok);
    REQUIRE(says(refused, "format-traversal"));
    REQUIRE_FALSE(leaves(refused, "parse_yaml"));
    REQUIRE_FALSE(leaves(refused, "allowed_builtins"));
}

TEST_CASE("a dunder inside a string literal still reaches the expanded document", "[eval_python]")
{
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    const outcome resolved = run(span_document("${'a literal containing __import__'}"),
                                 meios::eval_policy::fail, handle);
    REQUIRE(resolved.ok);
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
