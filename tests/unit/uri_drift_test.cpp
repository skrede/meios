#include "uri_table.h"

#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <filesystem>
#include <string_view>

namespace
{

// The published rule id is a fixed backticked token, never a section title: a title
// gets reworded and would take the binding with it.
constexpr std::string_view rule_marker = "`rule:";

struct scan
{
    std::set<std::string> slugs;
    std::size_t           markers;
};

void collect_slugs(const std::string &line, scan &found)
{
    for(std::string::size_type at = line.find(rule_marker); at != std::string::npos;
        at = line.find(rule_marker, at + 1))
    {
        ++found.markers;
        const std::string::size_type start = at + rule_marker.size();
        const std::string::size_type close = line.find('`', start);
        if(close != std::string::npos && close > start)
            found.slugs.insert(line.substr(start, close - start));
    }
}

scan document_slugs()
{
    scan found{ {}, 0 };
    std::ifstream in{ std::filesystem::path{ MEIOS_DOCS_DIR } / "asset-resolution.md" };
    std::string line;
    while(std::getline(in, line))
        collect_slugs(line, found);
    return found;
}

std::set<std::string> table_slugs()
{
    std::set<std::string> out;
    for(const uri::row &r : uri::load_rows())
        out.insert(r.rule);
    return out;
}

// to_string answers "unknown" for every value past the last enumerator, which is the
// only enumerator range the enum exposes.
std::set<std::string> code_spellings()
{
    std::set<std::string> out;
    for(int value = 0; value < 1024; ++value)
    {
        const std::string_view name = meios::to_string(static_cast<meios::diagnostic_code>(value));
        if(name == "unknown")
            break;
        out.insert(std::string{ name });
    }
    return out;
}

}

TEST_CASE("the asset document carries one rule marker per rule it publishes", "[uri][drift]")
{
    const scan found = document_slugs();
    REQUIRE_FALSE(found.slugs.empty());
    REQUIRE(found.markers == found.slugs.size());
}

TEST_CASE("the asset document and the uri table name the same rules", "[uri][drift]")
{
    const std::set<std::string> published = document_slugs().slugs;
    const std::set<std::string> executed = table_slugs();
    REQUIRE_FALSE(published.empty());
    REQUIRE_FALSE(executed.empty());
    REQUIRE(published == executed);
}

TEST_CASE("every diagnostic code the uri table names round-trips through to_string", "[uri][drift]")
{
    const std::set<std::string> spellings = code_spellings();
    REQUIRE_FALSE(spellings.empty());
    for(const uri::row &r : uri::load_rows())
    {
        if(r.code == uri::no_code)
            continue;
        INFO(r.fixture);
        REQUIRE(spellings.count(r.code) == 1);
    }
}

TEST_CASE("a uri row names a diagnostic code exactly when its verdict reports one", "[uri][drift]")
{
    const std::vector<uri::row> rows = uri::load_rows();
    REQUIRE_FALSE(rows.empty());
    for(const uri::row &r : rows)
    {
        INFO(r.fixture);
        REQUIRE(uri::declared_verdict(r.verdict));
        if(r.verdict == "accept")
            REQUIRE(r.code == uri::no_code);
        else
            REQUIRE(r.code != uri::no_code);
    }
}
