#include "native_member_fixture.h"

#include "meios/xacro/eval_parser.h"

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <vector>
#include <cstddef>
#include <string_view>

using native_member_test::evaluate;
using native_member_test::outcome;

namespace
{

bool colliding(const oracle::row &one)
{
    return one.fields.size() == 3 && one.fields[1] != one.fields[2];
}

}

TEST_CASE("the refusing member set is the measured one, name for name", "[native][member]")
{
    std::vector<std::string> measured;
    for(const oracle::row &one : oracle::load_rows("collisions.cases"))
        if(colliding(one))
            measured.push_back(one.fields.front());
    const std::span<const std::string_view> carried = meios::detail::colliding_members();

    REQUIRE(carried.size() == measured.size());
    for(std::size_t at = 0; at < measured.size(); ++at)
        CHECK(carried[at] == measured[at]);
}

TEST_CASE("every measured collision refuses as a member and reads as a subscript", "[native][member]")
{
    std::size_t at = 0;
    for(const oracle::row &one : oracle::load_rows("collisions.cases"))
    {
        const std::string name = one.fields.front();
        const std::string held = std::to_string(at++);
        INFO("name: " << name);
        const outcome member = evaluate("kit." + name);
        if(colliding(one))
        {
            REQUIRE(member.failed);
            CHECK(member.messages.front().find("['" + name + "']") != std::string::npos);
            CHECK(member.messages.front().find("mapping operation") != std::string::npos);
        }
        else
        {
            CHECK(member.rendered == held);
        }
        CHECK(evaluate("kit['" + name + "']").rendered == held);
    }
}

TEST_CASE("a member written with a leading double underscore refuses by the prefix", "[native][member]")
{
    const outcome dunder = evaluate("kit.__class__");

    CHECK(dunder.failed);
    REQUIRE_FALSE(dunder.messages.empty());
    CHECK(dunder.messages.front().find("leading '__'") != std::string::npos);
    CHECK(dunder.messages.front().find("__class__") == std::string::npos);
    CHECK(evaluate("kit['__class__']").rendered == "-1");
}

TEST_CASE("a collision refusal carries none of the mapping's other keys", "[native][member]")
{
    const outcome refused = evaluate("kit.keys");

    REQUIRE(refused.messages.size() == 1);
    CHECK(refused.messages.front().find("items") == std::string::npos);
    CHECK(refused.messages.front().find("popitem") == std::string::npos);
    CHECK(refused.messages.front().find("setdefault") == std::string::npos);
}
