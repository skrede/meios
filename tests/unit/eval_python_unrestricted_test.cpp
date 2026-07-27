#include <meios/eval/python_evaluator.h>
#include <meios/eval/unrestricted_python_evaluator.h>

#include <meios/xacro.h>

#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>

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

struct tally
{
    std::vector<std::string> messages;

    void operator()(meios::level lvl, const std::string &message)
    {
        if(lvl == meios::level::error)
            messages.push_back(message);
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message) { (*this)(lvl, message); }

    void operator()(meios::level lvl, meios::diagnostic_code, const meios::source_location &, const std::string &message) { (*this)(lvl, message); }
};

struct verdict
{
    bool refused;
    std::vector<std::string> messages;
};

template <typename E>
std::optional<std::string> evaluate(std::string_view expr, const meios::eval_scope &scope)
{
    E evaluator;
    meios::log_sink silent;
    return evaluator.eval_to_text(expr, scope, silent);
}

template <typename E>
verdict refuse_of(std::string_view expr, const meios::eval_scope &scope)
{
    tally counts;
    meios::log_sink_f sink{ std::ref(counts) };
    E evaluator;
    const bool declined = !evaluator.eval_to_text(expr, scope, sink)
        && evaluator.last_failure_kind() == meios::eval_failure_kind::refused;
    return { declined, std::move(counts.messages) };
}

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

    std::optional<std::string> fetch(std::string_view, const std::filesystem::path &) override { return m_text; }

    std::optional<std::string> m_text;
};

meios::eval_scope serving(std::optional<std::string> text)
{
    meios::eval_scope scope;
    scope.install_text_loader(meios::text_resource_loader{ std::make_unique<fixed_text>(std::move(text)) });
    return scope;
}

}

static_assert(meios::text_evaluator<meios::unrestricted_python_evaluator>);

// One expression per rule the guard enforces — dunder identifier, withheld builtin, formatting
// traversal — each refused by the restricted evaluator and evaluated by the unrestricted one.
TEST_CASE("every rule the restricted evaluator enforces is absent from the unrestricted one",
          "[eval_python]")
{
    const meios::eval_scope scope;
    const std::pair<const char *, std::string> paired[] = {
        { "__import__('os').getcwd()", std::filesystem::current_path().string() },
        { "getattr(1, 'real')", "1" },
        { "'{}'.format(2)", "2" },
    };
    for(const std::pair<const char *, std::string> &c : paired)
    {
        INFO(c.first);
        REQUIRE(refuse_of<meios::python_evaluator>(c.first, scope).refused);
        const std::optional<std::string> got =
            evaluate<meios::unrestricted_python_evaluator>(c.first, scope);
        REQUIRE(got);
        REQUIRE(*got == c.second);
    }
}

TEST_CASE("both evaluators render every golden corpus row identically", "[eval_python]")
{
    const meios::eval_scope scope;
    for(const char *file : { "arithmetic.cases", "functions.cases", "precedence.cases" })
    {
        for(const golden_case &c : load_cases(file))
        {
            INFO(file << ": " << c.expr);
            const std::optional<std::string> got =
                evaluate<meios::unrestricted_python_evaluator>(c.expr, scope);
            REQUIRE(got);
            REQUIRE(*got == c.expected);
            REQUIRE(got == evaluate<meios::python_evaluator>(c.expr, scope));
        }
    }
}

TEMPLATE_TEST_CASE("a yaml spec escaping containment is refused by either evaluator", "[eval_python]",
                   meios::python_evaluator, meios::unrestricted_python_evaluator)
{
    const meios::eval_scope scope = serving(std::nullopt);
    const verdict got = refuse_of<TestType>("load_yaml('../../../etc/passwd')", scope);
    REQUIRE(got.refused);
    REQUIRE(names(got, "uncontained-yaml-path"));
    REQUIRE(names(got, "../../../etc/passwd"));
}

TEMPLATE_TEST_CASE("a scope carrying no loader refuses a yaml request under either evaluator",
                   "[eval_python]", meios::python_evaluator, meios::unrestricted_python_evaluator)
{
    const meios::eval_scope bare;
    const verdict got = refuse_of<TestType>("load_yaml('/etc/passwd')", bare);
    REQUIRE(got.refused);
    REQUIRE(names(got, "uncontained-yaml-path"));
    REQUIRE(names(got, "/etc/passwd"));
}

TEMPLATE_TEST_CASE("a resolvable yaml spec loads its unit tags identically under either evaluator",
                   "[eval_python]", meios::python_evaluator, meios::unrestricted_python_evaluator)
{
    const meios::eval_scope scope = serving("a: !radians 1.5\n"
                                            "b: !degrees 180\n"
                                            "c: !meters 2.0\n"
                                            "d: !millimeters 1000\n"
                                            "e: !foot 1\n"
                                            "f: !inches 1\n");
    const char *tagged[] = {
        "abs(load_yaml('u')['a'] - 1.5) < 1e-9",    "abs(load_yaml('u')['b'] - pi) < 1e-9",
        "abs(load_yaml('u')['c'] - 2.0) < 1e-9",    "abs(load_yaml('u')['d'] - 1.0) < 1e-9",
        "abs(load_yaml('u')['e'] - 0.3048) < 1e-9", "abs(load_yaml('u')['f'] - 0.0254) < 1e-9",
    };
    for(const char *expr : tagged)
    {
        INFO(expr);
        const std::optional<std::string> got = evaluate<TestType>(expr, scope);
        REQUIRE(got);
        REQUIRE(*got == "True");
    }
}
