#include "generated_record.h"
#include "generated_verdict.h"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <vector>
#include <cstddef>
#include <filesystem>

#ifndef MEIOS_GENERATED_FIXTURE_DIR
    #error "MEIOS_GENERATED_FIXTURE_DIR must name the generated fixture tree"
#endif

namespace
{

const std::filesystem::path fixture_tree{MEIOS_GENERATED_FIXTURE_DIR};

const generated::generated_case *case_named(const generated::record &held, const std::string &id)
{
    for(const generated::generated_case &one : held.cases)
        if(one.id == id)
            return &one;
    return nullptr;
}

bool reviewed(const std::vector<oracle::row> &rows, const generated::generated_case &one, const std::string &upstream, const std::string &meios)
{
    for(const oracle::row &row : rows)
        if(row.fields.size() >= 3 && row.fields[0] == one.id)
            return row.fields[1] == upstream && row.fields[2] == meios;
    return false;
}

generated::record literal_record(const std::string &document)
{
    generated::record held;
    held.families["literal"] = generated::family{1, {"TEXT"}, "@TEXT@"};
    held.cases.push_back(generated::generated_case{"literal_001", "literal", document, "", "-", ""});
    return held;
}

std::string meios_literal(const std::string &document)
{
    const generated::record held = literal_record(document);
    return generated::meios_verdict(held, held.cases.front(), fixture_tree);
}

std::string upstream_literal(const std::string &upstream)
{
    return generated::upstream_verdict(generated::generated_case{"literal_001", "literal", "", upstream, "-", ""});
}

std::string upstream_refused(const std::string &cause, const std::string &refusal)
{
    return generated::upstream_verdict(generated::generated_case{"literal_001", "literal", "", "REFUSED", cause, refusal});
}

}

TEST_CASE("every generated case agrees with the pinned reference or is a reviewed divergence", "[generated]")
{
    const generated::record held        = generated::read_record();
    const std::vector<oracle::row> rows = generated::reviewed_rows();
    REQUIRE_FALSE(held.cases.empty());
    for(const generated::generated_case &one : held.cases)
    {
        const std::string upstream = generated::upstream_verdict(one);
        const std::string meios    = generated::meios_verdict(held, one, fixture_tree);
        REQUIRE_FALSE(upstream.empty());
        REQUIRE_FALSE(meios.empty());
        if(meios == upstream || reviewed(rows, one, upstream, meios))
            continue;
        FAIL_CHECK("unreviewed generated divergence\t" << one.id << '\t' << upstream << '\t' << meios << "\ttext: " << one.text);
    }
}

TEST_CASE("every reviewed generated divergence still disagrees as recorded", "[generated]")
{
    const generated::record held = generated::read_record();
    for(const oracle::row &row : generated::reviewed_rows())
    {
        REQUIRE(row.fields.size() == 4);
        INFO("reviewed row " << row.fields[0]);
        const generated::generated_case *one = case_named(held, row.fields[0]);
        REQUIRE(one != nullptr);
        CHECK(row.fields[1] == generated::upstream_verdict(*one));
        CHECK(row.fields[2] == generated::meios_verdict(held, *one, fixture_tree));
        CHECK(row.fields[1] != row.fields[2]);
    }
}

TEST_CASE("the record holds the committed count in every family", "[generated]")
{
    const generated::record held = generated::read_record();
    REQUIRE_FALSE(held.families.empty());
    std::map<std::string, std::size_t> counted;
    for(const generated::generated_case &one : held.cases)
        ++counted[one.family];
    for(const auto &[name, layout] : held.families)
    {
        INFO("family " << name);
        CHECK(layout.count > 0);
        CHECK(counted[name] == layout.count);
        CHECK_FALSE(layout.layout.empty());
    }
    CHECK(counted.size() == held.families.size());
}

TEST_CASE("the verdict is the whole canonical document on both sides", "[generated]")
{
    const std::string upstream = upstream_literal(R"(<robot name="g"><link name="a" type="t"/><visual/></robot>)");
    CHECK(upstream == meios_literal(R"(<robot name="g"><link type="t" name="a"/><visual/></robot>)"));
    CHECK(upstream != meios_literal(R"(<robot name="g"><link type="t" name="a"/></robot>)"));
    CHECK(upstream_literal(R"(<robot name="g"></robot>)") == meios_literal(R"(<robot name="g"/>)"));
}

TEST_CASE("two refusals agree only on the same cause", "[generated]")
{
    const std::string upstream = upstream_refused("XacroException NameError", "name 'nope' is not defined");
    const std::string named    = meios_literal(R"(<robot name="g" xmlns:xacro="http://www.ros.org/wiki/xacro"><link name="${nope}"/></robot>)");
    const std::string included = meios_literal(R"(<robot name="g" xmlns:xacro="http://www.ros.org/wiki/xacro"><xacro:include filename="missing.xacro"/></robot>)");
    CHECK(upstream == "REFUSED undefined_property");
    CHECK(named == upstream);
    CHECK(included == "REFUSED unresolved_include");
    CHECK(included != upstream);
    CHECK(upstream_literal(R"(<robot name="g"/>)") != named);
}

TEST_CASE("a staged path is normalized in the native and the generic spelling", "[generated]")
{
    const std::filesystem::path root = fixture_tree / "R";
    CHECK(generated::normalized(root.string() + "/p", fixture_tree) == "@ROOT@/p");
    CHECK(generated::normalized(root.generic_string() + "/p", fixture_tree) == "@ROOT@/p");
    CHECK(meios_literal(R"(<robot name="g"><link name=")" + root.generic_string() + R"(/p"/></robot>)") == upstream_literal(R"(<robot name="g"><link name="@ROOT@/p"/></robot>)"));
}
