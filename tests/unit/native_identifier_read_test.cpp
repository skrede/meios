#include "xacro_include_tree.h"

#include "meios/detail/utf8.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>

namespace
{

struct reading
{
    std::optional<std::string> document;
    meios::diagnostic_code code;
    std::string message;
};

using reader = reading(const std::string &);

std::string spelled(std::u32string_view points)
{
    std::string out;
    for(char32_t point : points)
        meios::detail::append_utf8(out, point);
    return out;
}

std::string replaced(std::string text, std::string_view from, std::string_view to)
{
    for(std::size_t at = text.find(from); !from.empty() && at != std::string::npos; at = text.find(from, at + to.size()))
        text.replace(at, from.size(), to);
    return text;
}

reading read_document(const std::string &text, const std::filesystem::path &document)
{
    meios::log_sink silent;
    meios::source_stack sources{};
    meios::eval_scope scope;
    const include_tree::expansion_result out = meios::expand(text, scope, sources, document, meios::expansion_limits{}, silent);
    if(out.has_value())
        return reading{out->document, meios::diagnostic_code::unspecified, std::string()};
    return reading{std::nullopt, out.error().code, out.error().message};
}

reading read_body(const std::string &body)
{
    return read_document(include_tree::robot(body), "inline.xacro");
}

// The body is read after an include of w.xacro as n, which binds fi and includes m.xacro as m.
reading read_in_tree(const std::string &body)
{
    const include_tree::tree held;
    held.write("m.xacro", include_tree::robot(R"(<xacro:property name="fi" value="6"/>)"));
    held.write("w.xacro", include_tree::robot(R"(<xacro:property name="fi" value="5"/><xacro:include filename="m.xacro" ns="m"/>)"));
    const std::filesystem::path top = held.write("top.xacro", include_tree::robot(R"(<xacro:include filename="w.xacro" ns="n"/>)" + body));
    return read_document(include_tree::read_file(top), top);
}

// Every '@' in the body stands for the spelling under test.
reading read_spelled(std::string_view body, std::u32string_view spelling, reader &read = read_body)
{
    return read(replaced(std::string(body), "@", spelled(spelling)));
}

// A refusal that quotes its expression quotes what the author wrote, so the written spelling is put
// back to the normalized one before the two messages are compared.
bool reads_as(std::string_view body, std::u32string_view written, std::u32string_view normalized, reader &read = read_body)
{
    const reading as_written    = read_spelled(body, written, read);
    const reading as_normalized = read_spelled(body, normalized, read);
    if(as_written.document || as_normalized.document)
        return as_written.document == as_normalized.document;
    return as_written.code == as_normalized.code && replaced(as_written.message, spelled(written), spelled(normalized)) == as_normalized.message;
}

bool renders(const reading &got, std::string_view link)
{
    return got.document && got.document->find("<link name=\"" + std::string(link) + "\"/>") != std::string::npos;
}

struct spelling_case
{
    std::string_view body;
    std::u32string_view written;
    std::u32string_view normalized;
    std::string_view rendered;
};

void require_reads(const spelling_case &one, reader &read = read_body)
{
    CAPTURE(one.body, spelled(one.written));
    REQUIRE(reads_as(one.body, one.written, one.normalized, read));
    if(!one.rendered.empty())
        REQUIRE(renders(read_spelled(one.body, one.written, read), one.rendered));
}

}

TEST_CASE("a property bound under a name outside ASCII reads under the built-in evaluator", "[xacro][identifier]")
{
    REQUIRE(renders(read_body(R"(<xacro:property name="π" value="1"/><link name="l${π}"/>)"), "l1"));
    REQUIRE(renders(read_body(R"(<link name="l${'π'}"/>)"), "lπ"));
}

TEST_CASE("an expression reads a name under its NFKC form, as Python reads it", "[xacro][identifier]")
{
    const spelling_case cases[] = {
            {R"(<xacro:property name="fi" value="7"/><link name="l${@}"/>)", U"ﬁ", U"fi", "l7"},
            {R"(<xacro:property name="a" value="7"/><link name="l${@}"/>)", U"ａ", U"a", "l7"},
            {"<xacro:property name=\"\u03A9\" value=\"5\"/><link name=\"l${@}\"/>", U"\u2126", U"\u03A9", "l5"},
            {"<xacro:property name=\"\u00E9\" value=\"7\"/><link name=\"l${@}\"/>", U"e\u0301", U"\u00E9", "l7"},
            {"<xacro:property name=\"x\u0301\" value=\"1\"/><link name=\"l${@}\"/>", U"x\u0301", U"x\u0301", "l1"},
            {R"(<xacro:property name="a1" value="2"/><link name="l${@}"/>)", U"a１", U"a1", "l2"},
            {R"(<link name="l${@}"/>)", U"ｐｉ", U"pi", "l3.141592653589793"},
            {R"(<link name="l${@(0)}"/>)", U"ｓｉｎ", U"sin", "l0.0"},
    };
    for(const spelling_case &one : cases)
        require_reads(one);
}

TEST_CASE("every place an expression names something reads the NFKC spelling", "[xacro][identifier]")
{
    const spelling_case cases[] = {
            {R"(<link name="l${'a b'.@()}"/>)", U"ｓｐｌｉｔ", U"split", ""},
            {R"(<link name="l${dict(@=3)['fi']}"/>)", U"ﬁ", U"fi", "l3"},
            {R"(<xacro:macro name="m" params="@"><link name="l${@}"/></xacro:macro><xacro:m @="3"/>)", U"π", U"π", "l3"},
            {R"x(<xacro:arg name="arabic" default="2"/><link name="l$(eval @ + 1)"/>)x", U"ａｒａｂｉｃ", U"arabic", "l3"},
            {R"(<link name="l${@}"/>)", U"_＿x", U"__x", ""},
    };
    for(const spelling_case &one : cases)
        require_reads(one);
}

TEST_CASE("a name after an include namespace reads the NFKC spelling at every step of the walk", "[xacro][identifier]")
{
    require_reads({R"(<link name="l${n.@}"/>)", U"ﬁ", U"fi", "l5"}, read_in_tree);
    require_reads({R"(<link name="l${n.@.fi}"/>)", U"ｍ", U"m", "l6"}, read_in_tree);
    require_reads({R"(<link name="l${n.m.@}"/>)", U"ﬁ", U"fi", "l6"}, read_in_tree);
    require_reads({R"(<link name="l${n.@}"/>)", U"ｍ.ﬁ", U"m.fi", "l6"}, read_in_tree);
    const reading space = read_spelled(R"(<link name="l${n.@}"/>)", U"ｍ", read_in_tree);
    REQUIRE_FALSE(space.document);
    REQUIRE(space.message.find("'n.m' names an include namespace") != std::string::npos);
    REQUIRE(reads_as(R"(<link name="l${n.@}"/>)", U"ｍ", U"m", read_in_tree));
}

TEST_CASE("a macro parameter is stored as written, so a decomposed one is not read under its composed form", "[xacro][identifier]")
{
    const reading got = read_body("<xacro:macro name=\"m\" params=\"e\u0301\"><link name=\"l${\u00E9}\"/></xacro:macro><xacro:m e\u0301=\"3\"/>");
    REQUIRE_FALSE(got.document);
    REQUIRE(got.code == meios::diagnostic_code::undefined_property);
    REQUIRE(got.message.find("name '\u00E9' is not defined") != std::string::npos);
}
