#include <meios/eval/python_evaluator.h>

#include <meios/xacro.h>
#include <meios/io/source_stack.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <sstream>
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

std::optional<std::string> evaluate(std::string_view expr, const meios::eval_scope &scope)
{
    meios::python_evaluator evaluator;
    meios::log_sink silent;
    return evaluator.eval_to_text(expr, scope, silent);
}

struct tally
{
    int errors{ 0 };

    void operator()(meios::level lvl, const std::string &)
    {
        if(lvl == meios::level::error)
            ++errors;
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &)
    {
        if(lvl == meios::level::error)
            ++errors;
    }
};

struct outcome
{
    bool ok;
    std::string document;
    int errors;
};

outcome run(std::string_view source, meios::eval_policy policy, meios::evaluator_handle *backend)
{
    tally counts;
    meios::log_sink_f sink{ std::ref(counts) };
    meios::eval_scope scope;
    meios::source_stack sources;
    meios::expansion out = meios::expand(source, scope, sources, "robot.xacro",
                                         meios::expansion_limits{}, policy, backend, sink);
    return outcome{ out.ok, std::move(out.document), counts.errors };
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
    meios::evaluator_handle handle{ meios::python_evaluator{} };
    const outcome resolved = run(span_document("${[i*i for i in range(4)]}"),
                                 meios::eval_policy::fail, &handle);
    REQUIRE(resolved.ok);
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
    const std::filesystem::path tmp =
        std::filesystem::temp_directory_path() / "meios_eval_python_load_yaml.yaml";
    std::ofstream(tmp) << "arm:\n  dof: 6\n";
    const std::string expr = "load_yaml('" + tmp.generic_string() + "')['arm']['dof']";
    const std::optional<std::string> got = evaluate(expr, meios::eval_scope{});
    std::filesystem::remove(tmp);
    REQUIRE(got);
    REQUIRE(*got == "6");
}

TEST_CASE("an injected python backend resolves a document construct", "[eval_python]")
{
    meios::evaluator_handle handle{ meios::python_evaluator{} };
    const outcome resolved = run(span_document("${[i*i for i in range(3)]}"),
                                 meios::eval_policy::fail, &handle);
    REQUIRE(resolved.ok);
    REQUIRE(leaves(resolved, "[0, 1, 4]"));
}

TEST_CASE("a non-python construct resolves identically with the backend injected", "[eval_python]")
{
    meios::evaluator_handle handle{ meios::python_evaluator{} };
    const outcome resolved = run(span_document("${1+1}"), meios::eval_policy::fail, &handle);
    REQUIRE(resolved.ok);
    REQUIRE(leaves(resolved, "<l>2</l>"));
}

TEST_CASE("a python runtime throw hard-fails even under skip", "[eval_python]")
{
    meios::evaluator_handle handle{ meios::python_evaluator{} };
    const outcome raised = run(span_document("${1/0}"), meios::eval_policy::skip, &handle);
    REQUIRE_FALSE(raised.ok);
    REQUIRE(raised.errors >= 1);
    REQUIRE_FALSE(leaves(raised, "${1/0}"));
}

TEST_CASE("xacro.load_yaml resolves a mapping through the SimpleNamespace shim", "[eval_python]")
{
    const std::filesystem::path tmp =
        std::filesystem::temp_directory_path() / "meios_eval_xacro_map.yaml";
    std::ofstream(tmp) << "arm:\n  dof: 6\n";
    const std::string expr = "xacro.load_yaml('" + tmp.generic_string() + "')['arm']['dof']";
    const std::optional<std::string> got = evaluate(expr, meios::eval_scope{});
    std::filesystem::remove(tmp);
    REQUIRE(got);
    REQUIRE(*got == "6");
}

TEST_CASE("a !degrees yaml tag loads as radians through the SafeLoader constructor", "[eval_python]")
{
    const std::filesystem::path tmp =
        std::filesystem::temp_directory_path() / "meios_eval_degrees.yaml";
    std::ofstream(tmp) << "angle: !degrees 180\n";
    const std::string expr =
        "abs(xacro.load_yaml('" + tmp.generic_string() + "')['angle'] - pi) < 1e-9";
    const std::optional<std::string> got = evaluate(expr, meios::eval_scope{});
    std::filesystem::remove(tmp);
    REQUIRE(got);
    REQUIRE(*got == "True");
}

TEST_CASE("a !radians yaml tag loads unchanged through the SafeLoader constructor", "[eval_python]")
{
    const std::filesystem::path tmp =
        std::filesystem::temp_directory_path() / "meios_eval_radians.yaml";
    std::ofstream(tmp) << "angle: !radians 1.5\n";
    const std::string expr =
        "abs(xacro.load_yaml('" + tmp.generic_string() + "')['angle'] - 1.5) < 1e-9";
    const std::optional<std::string> got = evaluate(expr, meios::eval_scope{});
    std::filesystem::remove(tmp);
    REQUIRE(got);
    REQUIRE(*got == "True");
}

TEST_CASE("a dict crossing a xacro:property boundary is re-hydrated for nested subscripts",
          "[eval_python]")
{
    const std::filesystem::path tmp =
        std::filesystem::temp_directory_path() / "meios_eval_rehydrate.yaml";
    std::ofstream(tmp) << "limits:\n  shoulder:\n    max: 42\n";
    meios::evaluator_handle handle{ meios::python_evaluator{} };
    const std::string document = std::string(header)
        + "<xacro:property name=\"config\" value=\"${xacro.load_yaml('" + tmp.generic_string()
        + "')}\"/>"
        + "<xacro:property name=\"section\" value=\"${config['limits']}\"/>"
        + "<l>${section['shoulder']['max']}</l></robot>";
    const outcome resolved = run(document, meios::eval_policy::fail, &handle);
    std::filesystem::remove(tmp);
    REQUIRE(resolved.ok);
    REQUIRE(leaves(resolved, "<l>42</l>"));
}

TEST_CASE("an ordinary string binding is never fabricated into a container", "[eval_python]")
{
    meios::eval_scope scope;
    scope.set("v", std::string("6"));
    const std::optional<std::string> got = evaluate("v + '0'", scope);
    REQUIRE(got);
    REQUIRE(*got == "60");
}

TEST_CASE("an authored literal-shaped property value stays a string under subscript",
          "[eval_python]")
{
    meios::evaluator_handle handle{ meios::python_evaluator{} };
    const std::string document = std::string(header)
        + "<xacro:property name=\"x\" value=\"[1, 2]\"/>"
        + "<l>${x[0]}</l></robot>";
    const outcome resolved = run(document, meios::eval_policy::fail, &handle);
    REQUIRE(resolved.ok);
    REQUIRE(leaves(resolved, "<l>[</l>"));
    REQUIRE_FALSE(leaves(resolved, "<l>1</l>"));
}

TEST_CASE("a container emitted whole into element text carries no control-char marker",
          "[eval_python]")
{
    meios::evaluator_handle handle{ meios::python_evaluator{} };
    const outcome resolved = run(span_document("${[1, 2, 3]}"), meios::eval_policy::fail, &handle);
    REQUIRE(resolved.ok);
    REQUIRE(leaves(resolved, "<l>[1, 2, 3]</l>"));
    REQUIRE(resolved.document.find('\x01') == std::string::npos);
}

TEST_CASE("a container emitted whole into an attribute value carries no control-char marker",
          "[eval_python]")
{
    meios::evaluator_handle handle{ meios::python_evaluator{} };
    const std::string document = std::string(header) + "<l tag=\"${[1, 2, 3]}\"/></robot>";
    const outcome resolved = run(document, meios::eval_policy::fail, &handle);
    REQUIRE(resolved.ok);
    REQUIRE(leaves(resolved, "tag=\"[1, 2, 3]\""));
    REQUIRE(resolved.document.find('\x01') == std::string::npos);
}

TEST_CASE("a list crossing a xacro:property boundary re-hydrates without leaking the marker",
          "[eval_python]")
{
    meios::evaluator_handle handle{ meios::python_evaluator{} };
    const std::string document = std::string(header)
        + "<xacro:property name=\"row\" value=\"${[10, 20, 30]}\"/>"
        + "<l>${row[1]}</l></robot>";
    const outcome resolved = run(document, meios::eval_policy::fail, &handle);
    REQUIRE(resolved.ok);
    REQUIRE(leaves(resolved, "<l>20</l>"));
    REQUIRE(resolved.document.find('\x01') == std::string::npos);
}
