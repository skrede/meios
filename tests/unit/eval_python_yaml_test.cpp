#include "eval_python_refusal_fixture.h"

#include <catch2/catch_test_macros.hpp>

namespace
{

// The refusal is driven through the evaluator with the real acquisition loader installed, because
// a record emitted above the loader is invisible to anything that calls the loader itself.
struct counted_refusal
{
    bool refused;
    std::vector<meios::captured_diagnostic> records;
};

counted_refusal count_refusal(std::string_view expr, meios::eval_scope &scope,
                              meios::capturing_log_sink &capture)
{
    meios::python_evaluator evaluator;
    const bool declined = !evaluator.eval_to_text(expr, scope, capture)
        && evaluator.last_failure_kind() == meios::eval_failure_kind::refused;
    return { declined, capture.records() };
}

std::size_t coded(const std::vector<meios::captured_diagnostic> &records, meios::diagnostic_code code)
{
    return static_cast<std::size_t>(
        std::count_if(records.begin(), records.end(),
                      [code](const meios::captured_diagnostic &d) { return d.code == code; }));
}

std::size_t mentioning(const std::vector<meios::captured_diagnostic> &records,
                       std::string_view fragment)
{
    return static_cast<std::size_t>(
        std::count_if(records.begin(), records.end(), [fragment](const meios::captured_diagnostic &d) {
            return d.message.find(fragment) != std::string::npos;
        }));
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

TEST_CASE("one refused yaml acquisition produces one record carrying the cause", "[eval_python]")
{
    meios::log_sink silent;
    meios::capturing_log_sink capture{ silent };
    meios::source_stack sources;
    const std::vector<std::filesystem::path> roots{ std::filesystem::temp_directory_path() };
    meios::eval_scope scope;
    scope.install_text_loader(meios::detail::make_yaml_text_loader(sources, roots, capture));

    const counted_refusal got =
        count_refusal("load_yaml('../escaped-limits.yaml')", scope, capture);

    for(const meios::captured_diagnostic &record : got.records)
        UNSCOPED_INFO(meios::to_string(record.code) << " | " << record.message);
    CHECK(got.refused);
    CHECK(got.records.size() == 2);
    CHECK(coded(got.records, meios::diagnostic_code::uncontained_asset) == 1);
    CHECK(mentioning(got.records, "uncontained-yaml-path") == 1);
}

TEST_CASE("a spec named through a variable is still named by the surviving record", "[eval_python]")
{
    meios::log_sink silent;
    meios::capturing_log_sink capture{ silent };
    meios::source_stack sources;
    const std::vector<std::filesystem::path> roots{ std::filesystem::temp_directory_path() };
    meios::eval_scope scope;
    scope.install_text_loader(meios::detail::make_yaml_text_loader(sources, roots, capture));
    scope.set("limits_file", meios::binding{ std::string("../escaped-limits.yaml") });

    const counted_refusal got = count_refusal("xacro.load_yaml(limits_file)", scope, capture);

    for(const meios::captured_diagnostic &record : got.records)
        UNSCOPED_INFO(meios::to_string(record.code) << " | " << record.message);
    CHECK(got.refused);
    CHECK(mentioning(got.records, "escaped-limits.yaml") >= 1);
    CHECK(coded(got.records, meios::diagnostic_code::uncontained_asset) == 1);
}
