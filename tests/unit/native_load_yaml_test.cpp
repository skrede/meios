#include "load_yaml_probe.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <utility>
#include <string_view>

using load_yaml::evaluate;
using load_yaml::names;
using load_yaml::reading;

TEST_CASE("the loading call reads a document named by a bare name, a literal or a composition",
          "[native][yaml][load_yaml]")
{
    SECTION("a bare name naming a document still loads it")
    {
        CHECK(evaluate("xacro.load_yaml(catalogue_file)['name']").rendered == "parts");
    }

    SECTION("a quoted literal names the same document")
    {
        CHECK(evaluate("xacro.load_yaml('catalogue.yaml')['name']").rendered == "parts");
    }

    // The vendor spelling, minus the package locator the span layer resolves before the
    // expression runs: a literal, a property and a literal, joined.
    SECTION("a concatenation of a literal, a property and a literal names the document it composes")
    {
        const reading got = evaluate("xacro.load_yaml('cfg/' + stem + '.yaml')['arm']['length']");
        CHECK_FALSE(got.failed);
        CHECK(got.rendered == "0.12");
        CHECK(got.loads == std::vector<std::string>{ "cfg/arm.yaml" });
    }
}

TEST_CASE("an argument that does not yield text refuses naming the kind it produced",
          "[native][yaml][load_yaml]")
{
    const std::pair<std::string_view, std::string_view> rows[] = {
        { "xacro.load_yaml(1)", "integer" },
        { "xacro.load_yaml(1.5)", "real" },
        { "xacro.load_yaml(True)", "boolean" },
        { "xacro.load_yaml(xacro.load_yaml(catalogue_file)['empty'])", "null" },
        { "xacro.load_yaml(xacro.load_yaml(catalogue_file)['items'])", "sequence" },
        { "xacro.load_yaml(xacro.load_yaml(catalogue_file))", "mapping" }
    };

    for(const std::pair<std::string_view, std::string_view> &row : rows)
    {
        INFO(row.first << " => " << row.second);
        const reading got = evaluate(row.first);
        CHECK(got.failed);
        CHECK(names(got, std::string("named by text, not by a ") + std::string(row.second)));
    }
}

TEST_CASE("an argument yielding an unreachable name refuses as an unresolved asset",
          "[native][yaml][load_yaml]")
{
    const reading got = evaluate("xacro.load_yaml('cfg/' + 'absent' + '.yaml')");

    CHECK(got.failed);
    CHECK(names(got, "cannot reach the auxiliary document \"cfg/absent.yaml\""));
    CHECK(got.codes
          == std::vector<meios::diagnostic_code>{ meios::diagnostic_code::unresolved_asset });
}

TEST_CASE("an undefined name in the argument refuses as any undefined name does",
          "[native][yaml][load_yaml]")
{
    const reading got = evaluate("xacro.load_yaml(nowhere)");

    CHECK(got.failed);
    CHECK(names(got, "name 'nowhere' is not defined"));
    CHECK(got.codes
          == std::vector<meios::diagnostic_code>{ meios::diagnostic_code::undefined_property });
    CHECK(got.loads.empty());
}

TEST_CASE("a missing closing parenthesis and a second argument each refuse",
          "[native][yaml][load_yaml]")
{
    for(std::string_view spelling :
        { "xacro.load_yaml('catalogue.yaml'", "xacro.load_yaml('catalogue.yaml', 'arm.yaml')" })
    {
        INFO(spelling);
        const reading got = evaluate(spelling);
        CHECK(got.failed);
        CHECK(names(got, "one argument yielding text"));
    }
}


// The containment, read off the calls: every document that arrived came through the scope's own
// loader, and a scope carrying none reaches none at all, however the name was composed.
TEST_CASE("bytes reach the parser only through the scope's document-relative loader",
          "[native][yaml][load_yaml]")
{
    const reading through =
        evaluate("xacro.load_yaml('catalogue.yaml')['name'] + "
                 "xacro.load_yaml('cfg/' + stem + '.yaml')['arm']['length']");
    CHECK(through.loads == std::vector<std::string>{ "catalogue.yaml", "cfg/arm.yaml" });

    const reading without = evaluate("xacro.load_yaml('cfg/' + stem + '.yaml')", false);
    CHECK(without.failed);
    CHECK(without.loads.empty());
    CHECK(names(without, "cannot reach the auxiliary document \"cfg/arm.yaml\""));
}
