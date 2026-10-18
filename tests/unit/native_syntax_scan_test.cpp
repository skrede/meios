#include "branch_probe.h"

#include "meios/xacro/lexer.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>
#include <string_view>

namespace
{

std::vector<meios::detail::token_kind> kinds_of(std::string_view source)
{
    std::vector<meios::detail::token_kind> kinds;
    for(const meios::detail::token &one : meios::detail::tokenize(source))
        kinds.push_back(one.kind);
    return kinds;
}

}

TEST_CASE("a bare assignment character scans as its own kind and the scan carries on", "[native][syntax]")
{
    using kind = meios::detail::token_kind;

    CHECK(kinds_of("a = 1") == std::vector<kind>{kind::name, kind::assign, kind::number, kind::end});
    CHECK(kinds_of("dict(a=1, b=2)") ==
          std::vector<kind>{kind::name, kind::lparen, kind::name, kind::assign, kind::number, kind::comma, kind::name, kind::assign, kind::number, kind::rparen, kind::end});
}

// The two-character probe runs before the one-character table, and that ordering is what keeps
// the equality operator whole now that its character also stands alone. Reversing the two would
// scan every "==" as a pair of assignments, which these cases refuse.
TEST_CASE("the equality operator still scans as exactly one token wherever it stands", "[native][syntax]")
{
    using kind = meios::detail::token_kind;

    CHECK(kinds_of("a == b") == std::vector<kind>{kind::name, kind::equal_equal, kind::name, kind::end});
    CHECK(kinds_of("1==2") == std::vector<kind>{kind::number, kind::equal_equal, kind::number, kind::end});
    CHECK(kinds_of("a==1") == std::vector<kind>{kind::name, kind::equal_equal, kind::number, kind::end});
    CHECK(kinds_of("'a'=='b'") == std::vector<kind>{kind::string, kind::equal_equal, kind::string, kind::end});
    CHECK(kinds_of("a === b") == std::vector<kind>{kind::name, kind::equal_equal, kind::assign, kind::name, kind::end});
    CHECK(kinds_of("a != b") == std::vector<kind>{kind::name, kind::not_equal, kind::name, kind::end});
    CHECK(kinds_of("a <= b >= c") == std::vector<kind>{kind::name, kind::less_equal, kind::name, kind::greater_equal, kind::name, kind::end});
    CHECK(probe::evaluate("1 == 1").rendered == "True");
}

TEST_CASE("an assignment no constructor call consumes still refuses", "[native][syntax]")
{
    const probe::outcome loose = probe::evaluate("1 = 2");

    CHECK(loose.failed);
    REQUIRE_FALSE(loose.messages.empty());
    CHECK(loose.messages.front().find("trailing tokens") != std::string::npos);
    CHECK(probe::evaluate("= 1").failed);
    CHECK(probe::evaluate("max(1 = 2)").failed);
}
