#include "oracle_records.h"

#include "meios/xacro/xacro_attributes.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/capturing_log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <optional>
#include <algorithm>
#include <string_view>

namespace
{

using meios::diagnostic_code;
using meios::detail::attribute_row;
using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

struct heard_expansion
{
    expansion_result out;
    std::vector<meios::captured_diagnostic> records;
};

heard_expansion expand_heard(const std::string &body)
{
    meios::log_sink quiet;
    meios::capturing_log_sink heard(quiet);
    meios::source_stack sources{};
    meios::eval_scope scope;
    const std::string document = "<robot name=\"probe\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">\n" + body + "\n</robot>";
    expansion_result out = meios::expand(document, scope, sources, "attributes.xacro", meios::expansion_limits{}, heard);
    return {std::move(out), heard.records()};
}

void require_structural_refusal(const expansion_result &out, std::int32_t line)
{
    REQUIRE_FALSE(out.has_value());
    CHECK(out.error().code == diagnostic_code::xacro_structural_error);
    CHECK(out.error().loc.line == line);
}

std::size_t unknown_attribute_warnings(const heard_expansion &heard)
{
    return static_cast<std::size_t>(std::ranges::count(heard.records, diagnostic_code::unknown_attribute, &meios::captured_diagnostic::code));
}

std::string record_id(std::string_view tag)
{
    std::string id = "attrs.";
    for(const char c : tag)
        id += c == ':' ? '_' : c;
    return id;
}

std::string quoted(std::string_view required, std::string_view optional)
{
    return "required=" + std::string(required) + " optional=" + std::string(optional);
}

}

// The record lists `default` as required for xacro:arg; here a declaration without one is a
// required argument, so the table lists it as optional instead.
TEST_CASE("the attribute table pairs one-to-one with the record's attribute lists", "[xacro][attributes]")
{
    const std::vector<oracle::row> &rows = oracle::load_rows("grammar_constructs.cases");
    const std::size_t recorded = static_cast<std::size_t>(std::ranges::count_if(rows, [](const oracle::row &one) { return one.fields.size() > 1 && one.fields[1] == "attrs"; }));
    REQUIRE(recorded == meios::detail::attribute_rows().size());
    for(const attribute_row &row : meios::detail::attribute_rows())
    {
        INFO(row.tag);
        const std::optional<oracle::row> found = oracle::lookup(rows, record_id(row.tag));
        REQUIRE(found);
        REQUIRE(found->fields.size() == 5);
        if(row.tag == "xacro:arg")
        {
            CHECK(found->fields[4] == quoted("name,default", ""));
            CHECK(quoted(row.required, row.optional) == quoted("name", "default"));
        }
        else
            CHECK(found->fields[4] == quoted(row.required, row.optional));
    }
}

TEST_CASE("a macro definition with no name refuses at its line", "[xacro][attributes]")
{
    require_structural_refusal(expand_heard("<link name=\"a\"/>\n<xacro:macro><link name=\"l\"/></xacro:macro>").out, 3);
}

TEST_CASE("a macro definition with an empty name is not refused as missing one", "[xacro][attributes]")
{
    const heard_expansion heard = expand_heard(R"(<xacro:macro name=""><link name="e"/></xacro:macro><link name="l"/>)");
    REQUIRE(heard.out.has_value());
    CHECK(heard.out->document.find(R"(<link name="l"/>)") != std::string::npos);
    CHECK(unknown_attribute_warnings(heard) == 0);
}

TEST_CASE("an attribute outside a conditional's list warns at the conditional", "[xacro][attributes]")
{
    const heard_expansion heard = expand_heard("<link name=\"a\"/>\n<xacro:if value=\"1\" doc=\"d\"><link name=\"t\"/></xacro:if>");
    REQUIRE(heard.out.has_value());
    CHECK(heard.out->document.find(R"(<link name="t"/>)") != std::string::npos);
    REQUIRE(unknown_attribute_warnings(heard) == 1);
    CHECK(heard.records.front().message == "<xacro:if> carries an attribute 'doc' it does not read");
    CHECK(heard.records.front().loc.line == 3);
}

TEST_CASE("each unknown attribute warns once, in document order, after the required ones are found", "[xacro][attributes]")
{
    const heard_expansion heard = expand_heard(R"(<xacro:macro name="m" doc="d" foo="f"><link name="m"/></xacro:macro><xacro:m/>)");
    REQUIRE(heard.out.has_value());
    REQUIRE(unknown_attribute_warnings(heard) == 2);
    CHECK(heard.records[0].message == "<xacro:macro> carries an attribute 'doc' it does not read");
    CHECK(heard.records[1].message == "<xacro:macro> carries an attribute 'foo' it does not read");
    const heard_expansion missing = expand_heard(R"(<xacro:macro params="" doc="d"/><link name="l"/>)");
    require_structural_refusal(missing.out, 2);
    CHECK(unknown_attribute_warnings(missing) == 0);
}

TEST_CASE("a call, an element and an attribute read their other attributes and never warn", "[xacro][attributes]")
{
    for(const attribute_row &row : meios::detail::attribute_rows())
    {
        INFO(row.tag);
        const bool content = row.tag == "xacro:call" || row.tag == "xacro:element" || row.tag == "xacro:attribute";
        CHECK(row.warns != content);
    }
}

TEST_CASE("an include without a filename refuses at the include before any file is looked up", "[xacro][attributes]")
{
    const expansion_result out = expand_heard(R"(<xacro:include ns="n"/><link name="l"/>)").out;
    require_structural_refusal(out, 2);
    CHECK(out.error().message == "<xacro:include> needs a filename attribute");
}

TEST_CASE("an argument declaration refuses without a name and with an empty one, and binds nothing without a default", "[xacro][attributes]")
{
    const expansion_result absent = expand_heard(R"(<xacro:arg default="1"/><link name="l"/>)").out;
    require_structural_refusal(absent, 2);
    CHECK(absent.error().message == "<xacro:arg> needs a name attribute");
    const expansion_result empty = expand_heard(R"(<xacro:arg name="" default="1"/><link name="l"/>)").out;
    require_structural_refusal(empty, 2);
    CHECK(empty.error().message == "<xacro:arg> has an empty name");
    const heard_expansion bare = expand_heard(R"(<xacro:arg name="a"/><link name="l"/>)");
    REQUIRE(bare.out.has_value());
    CHECK(bare.records.empty());
}

TEST_CASE("a block insertion without a name refuses at the insertion", "[xacro][attributes]")
{
    const char *body = "<xacro:macro name=\"m\" params=\"*b\">\n<xacro:insert_block/></xacro:macro><xacro:m><link name=\"b\"/></xacro:m>";
    require_structural_refusal(expand_heard(body).out, 3);
}
