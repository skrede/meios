#include "meios/xacro/eval_session.h"
#include "meios/xacro/substitution_detail.h"

#include <meios/xacro.h>

#include <meios/urdf/load.h>

#include <meios/io/source_stack.h>

#include <catch2/catch_test_macros.hpp>

#ifdef MEIOS_TEST_HAS_YAML
    #include <meios/yaml/parser.h>
#endif

#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <optional>
#include <filesystem>
#include <functional>
#include <string_view>

namespace
{

struct recorder
{
    std::vector<std::string> messages;

    void operator()(meios::level lvl, const std::string &message)
    {
        if(lvl == meios::level::error)
            messages.push_back(message);
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        (*this)(lvl, message);
    }

    void operator()(meios::level lvl, meios::diagnostic_code, const meios::source_location &,
                    const std::string &message)
    {
        (*this)(lvl, message);
    }
};

class fixed_loader final : public meios::text_resource_loader::fetcher
{
public:
    explicit fixed_loader(std::string text) : m_text(std::move(text)) {}

    std::optional<std::string> fetch(std::string_view, const std::filesystem::path &) override
    {
        return m_text;
    }

private:
    std::string m_text;
};

struct run
{
    bool survived;
    std::string text;
    std::vector<std::string> messages;
};

// The whole path a description takes: a name in the scope, the contained loader for the bytes,
// the installed parser for the value, and the load's own session for the ceilings.
run substitute(std::string_view document,
               const std::shared_ptr<const meios::yaml_parser_handle> &parser,
               const meios::evaluator_limits &ceilings, meios::eval_policy policy)
{
    meios::eval_scope scope;
    scope.install_text_loader(
        meios::text_resource_loader{ std::make_unique<fixed_loader>(std::string(document)) });
    scope.install_yaml_parser(parser);
    scope.set("params_file", meios::value{ std::string("params.yaml") });
    meios::detail::eval_session session(ceilings);
    meios::source_stack sources;
    recorder heard;
    meios::log_sink_f sink{ std::ref(heard) };
    const meios::expected<meios::substitution, meios::expansion_error> out =
        meios::detail::substitute_refined("value ${xacro.load_yaml(params_file)['a']}", scope,
                                          sources, "inline.xacro", policy, {}, session, sink, {});
    return run{ out.has_value(), out.has_value() ? out.value().text : std::string(),
                heard.messages };
}

constexpr std::string_view nested_document = "a:\n  b:\n    c: 1\n";

#ifdef MEIOS_TEST_HAS_YAML

struct probe
{
    std::optional<meios::value> parsed;
    meios::yaml_failure failure;
    std::vector<std::string> messages;
    meios::evaluator_counters counters;
};

probe read(std::string_view document, const meios::evaluator_limits &ceilings)
{
    const std::shared_ptr<const meios::yaml_parser_handle> parser = meios::make_yaml_parser();
    meios::evaluator_counters counters;
    recorder heard;
    meios::log_sink_f sink{ std::ref(heard) };
    const meios::yaml_outcome out = (*parser)(document, ceilings, counters, sink, {});
    return probe{ out.parsed, out.failure, heard.messages, counters };
}

probe read(std::string_view document)
{
    const meios::evaluator_limits defaults;
    return read(document, defaults);
}

run substitute(std::string_view document, const meios::evaluator_limits &ceilings,
               meios::eval_policy policy)
{
    return substitute(document, meios::make_yaml_parser(), ceilings, policy);
}

#endif

}

TEST_CASE("default load options carry a parser exactly when the module is built",
          "[native][yaml]")
{
    const meios::load_options opts;

#ifdef MEIOS_HAS_YAML
    REQUIRE(opts.yaml);
    CHECK(opts.yaml->valid());
#else
    CHECK_FALSE(opts.yaml);
#endif
}

TEST_CASE("an explicitly chosen parser survives construction", "[native][yaml]")
{
    meios::load_options opts;
    const std::shared_ptr<const meios::yaml_parser_handle> empty =
        std::make_shared<const meios::yaml_parser_handle>();
    opts.yaml = empty;

    CHECK(opts.yaml == empty);
}

// The degradation the option governs, stated end to end and from the caller's side: options a
// caller constructs and does nothing else to carry whatever this build can do, and a build that
// can do nothing says so by name rather than failing to link or resolving to something invented.
TEST_CASE("a description reading an auxiliary document resolves or names what is missing",
          "[native][yaml]")
{
    const meios::load_options opts;
    const meios::evaluator_limits ceilings;
    const run out = substitute("a: 1\n", opts.yaml, ceilings, meios::eval_policy::fail);

#ifdef MEIOS_HAS_YAML
    CHECK(out.survived);
    CHECK(out.text == "value 1");
#else
    CHECK_FALSE(out.survived);
    REQUIRE_FALSE(out.messages.empty());
    CHECK(out.messages.front().find("resolves no auxiliary document format") != std::string::npos);
#endif
}

#ifdef MEIOS_TEST_HAS_YAML

TEST_CASE("the factory yields a parser the seam accepts", "[native][yaml]")
{
    const std::shared_ptr<const meios::yaml_parser_handle> parser = meios::make_yaml_parser();

    REQUIRE(parser);
    CHECK(parser->valid());
}

TEST_CASE("a mapping keeps its source order and reads by key", "[native][yaml]")
{
    const probe out = read("zulu: 1\nalpha: 2\nmike: 3\n");

    REQUIRE(out.parsed.has_value());
    REQUIRE(out.parsed->kind() == meios::value_kind::mapping);
    REQUIRE(out.parsed->size() == 3);
    CHECK(out.parsed->key_at(0).value_or("") == "zulu");
    CHECK(out.parsed->key_at(1).value_or("") == "alpha");
    CHECK(out.parsed->key_at(2).value_or("") == "mike");
    CHECK(out.parsed->at("alpha")->integer().value_or(0) == 2);
}

TEST_CASE("a nested mapping converts through every level", "[native][yaml]")
{
    const probe out = read(nested_document);

    REQUIRE(out.parsed.has_value());
    const std::optional<meios::value> inner = out.parsed->at("a");
    REQUIRE(inner.has_value());
    REQUIRE(inner->at("b").has_value());
    CHECK(inner->at("b")->at("c")->integer().value_or(0) == 1);
    CHECK(out.counters.yaml_depth == 3);
    CHECK(out.counters.bytes == nested_document.size());
}

TEST_CASE("a construct outside the read surface refuses by name", "[native][yaml]")
{
    const std::pair<std::string_view, std::string_view> refusals[] = {
        { "a: &anchor 1\nb: *anchor\n", "an alias" },
        { "base: &b\n  x: 1\na:\n  <<: *b\n", "a merge key" },
        { "? [1, 2]\n: 3\n", "a sequence as a mapping key" },
        { "1: one\n", "a non-string mapping key" },
        { "a: 1\na: 2\n", "a duplicate key" },
    };
    for(const std::pair<std::string_view, std::string_view> &one : refusals)
    {
        INFO("document [" << one.first << ']');
        const probe out = read(one.first);
        CHECK_FALSE(out.parsed.has_value());
        REQUIRE(out.messages.size() == 1);
        CHECK(out.messages.front().find(one.second) != std::string::npos);
        CHECK(out.messages.front().find("line") != std::string::npos);
    }
}

TEST_CASE("a malformed document reports a parse fault rather than a partial value",
          "[native][yaml]")
{
    const probe out = read("a: *nope\n");

    CHECK_FALSE(out.parsed.has_value());
    CHECK(out.failure == meios::yaml_failure::refused);
    REQUIRE(out.messages.size() == 1);
    CHECK(out.messages.front().find("does not parse") != std::string::npos);
}

TEST_CASE("each auxiliary ceiling is charged before the work it bounds", "[native][yaml]")
{
    const meios::evaluator_limits nodes{ 2, 0, 0, 0, 0 };
    const probe over_nodes = read(nested_document, nodes);
    CHECK_FALSE(over_nodes.parsed.has_value());
    CHECK(over_nodes.counters.yaml_nodes <= nodes.yaml_nodes);

    const meios::evaluator_limits depth{ 0, 2, 0, 0, 0 };
    const probe over_depth = read(nested_document, depth);
    CHECK_FALSE(over_depth.parsed.has_value());
    CHECK(over_depth.counters.yaml_depth <= depth.yaml_depth);

    const meios::evaluator_limits bytes{ 0, 0, 4, 0, 0 };
    const probe over_bytes = read(nested_document, bytes);
    CHECK_FALSE(over_bytes.parsed.has_value());
    CHECK(over_bytes.counters.bytes == 0);
}

TEST_CASE("a crossed auxiliary ceiling is terminal under every evaluation policy",
          "[native][yaml]")
{
    const meios::evaluator_limits ceilings[] = {
        meios::evaluator_limits{ 2, 0, 0, 0, 0 },
        meios::evaluator_limits{ 0, 2, 0, 0, 0 },
        meios::evaluator_limits{ 0, 0, 4, 0, 0 },
    };
    for(const meios::evaluator_limits &crossed : ceilings)
    {
        for(meios::eval_policy policy :
            { meios::eval_policy::fail, meios::eval_policy::warn, meios::eval_policy::skip })
        {
            const meios::evaluator_limits roomy;
            CHECK(substitute("a: 1\n", roomy, policy).survived);
            CHECK_FALSE(substitute(nested_document, crossed, policy).survived);
        }
    }
}

// The one distinction leniency turns on: a construct a fuller backend would read is
// unsupported syntax and a lenient policy may retain the span, while a malformed document is
// invalid input and terminates whatever the policy says.
TEST_CASE("a malformed document terminates where an unread construct may be retained",
          "[native][yaml]")
{
    const meios::evaluator_limits roomy;

    CHECK_FALSE(substitute("a: *nope\n", roomy, meios::eval_policy::skip).survived);

    const run retained = substitute("a: &x 1\nb: *x\n", roomy, meios::eval_policy::skip);
    CHECK(retained.survived);
    CHECK(retained.text.find("xacro.load_yaml") != std::string::npos);
    CHECK_FALSE(substitute("a: &x 1\nb: *x\n", roomy, meios::eval_policy::fail).survived);
}

#endif
