#include "container_probe.h"
#include "container_table.h"

#include <meios/xacro/container_marker.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <optional>

namespace
{

std::string reported(const container::reading &got)
{
    std::string joined;
    for(const std::string &message : got.messages)
        joined += message + '\n';
    return joined;
}

// The subscript spelling of a trailing key, rewritten as the dotted spelling of the same key on
// the same mapping, which is how the shadowing pair's two halves are recognized as a pair.
std::optional<std::string> dotted_twin(const std::string &expr)
{
    const std::size_t open = expr.rfind("['");
    if(open == std::string::npos || expr.size() < open + 5 || expr.substr(expr.size() - 2) != "']")
        return std::nullopt;
    return expr.substr(0, open) + '.' + expr.substr(open + 2, expr.size() - open - 4);
}

bool carries_marker(const std::string &subject)
{
    for(char marker : meios::detail::container_markers)
    {
        if(subject.find(marker) != std::string::npos)
            return true;
    }
    return false;
}

}

TEST_CASE("the verdict column is exactly the vocabulary the loader allows", "[eval_python]")
{
    const std::vector<container::row> rows = container::load_rows();
    REQUIRE(rows.size() >= 25);
    for(const container::row &r : rows)
    {
        INFO(r.subject << "  |  " << r.expr);
        REQUIRE(container::known(r.verdict));
        REQUIRE_FALSE(r.expect.empty());
        REQUIRE_FALSE(r.note.empty());
    }
}

TEST_CASE("every accepting row renders the leaf it claims", "[eval_python]")
{
    for(const container::row &r : container::load_rows())
    {
        if(r.verdict != "accept")
            continue;
        INFO(r.subject << "  |  " << r.expr);
        const container::reading got = container::probe(r);
        INFO(reported(got));
        REQUIRE(got.leaf.has_value());
        REQUIRE(*got.leaf == r.expect);
    }
}

TEST_CASE("every refusing row produces no document and says why", "[eval_python]")
{
    for(const container::row &r : container::load_rows())
    {
        if(r.verdict != "refuse")
            continue;
        INFO(r.subject << "  |  " << r.expr);
        const container::reading got = container::probe(r);
        INFO(reported(got));
        REQUIRE_FALSE(got.leaf.has_value());
        REQUIRE(container::names(got, r.expect));
    }
}

TEST_CASE("a key colliding with a mapping method is written as both halves of a pair",
          "[eval_python]")
{
    const std::vector<container::row> rows = container::load_rows();
    int pairs = 0;
    for(const container::row &accepted : rows)
    {
        const std::optional<std::string> twin =
            accepted.verdict == "accept" ? dotted_twin(accepted.expr) : std::nullopt;
        if(!twin)
            continue;
        for(const container::row &refused : rows)
        {
            if(refused.verdict == "refuse" && refused.expr == *twin)
                ++pairs;
        }
    }
    REQUIRE(pairs >= 1);
}

TEST_CASE("the forged rows still carry a marker and still refuse", "[eval_python]")
{
    int forged = 0;
    for(const container::row &r : container::load_rows())
    {
        if(!carries_marker(r.subject))
            continue;
        INFO(r.expr);
        REQUIRE(r.verdict == "refuse");
        ++forged;
    }
    REQUIRE(forged >= 2);
}
