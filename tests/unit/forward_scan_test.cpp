#include "meios/detail/text_location.h"

#include <pugixml.hpp>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>
#include <utility>
#include <string_view>

namespace
{

// Parses a start tag and hands the helper the very bytes pugixml assigned offsets
// against, so offset_debug and the raw scan agree.
struct fixture
{
    std::string text;
    pugi::xml_document doc;

    explicit fixture(std::string source) : text(std::move(source))
    {
        REQUIRE(doc.load_buffer(text.data(), text.size()));
    }

    pugi::xml_node element() const { return doc.first_child(); }

    meios::source_location anchor() const
    {
        return meios::detail::node_location(element(), text, "doc.xml");
    }

    int column_of(char needle) const
    {
        const std::size_t at = text.find(needle);
        REQUIRE(at != std::string::npos);
        return meios::detail::offset_location(text, static_cast<std::ptrdiff_t>(at), "doc.xml").column;
    }

    int column_at(std::size_t offset) const
    {
        return meios::detail::offset_location(text, static_cast<std::ptrdiff_t>(offset), "doc.xml").column;
    }
};

meios::source_location refine(const fixture &f, std::size_t attr_index, std::size_t decoded_offset)
{
    return meios::detail::refine_attr_column(f.text, f.element(), attr_index, decoded_offset,
                                             f.anchor());
}

}

TEST_CASE("forward scan confirms the token column in a double-quoted value")
{
    fixture f("<a radius=\"${bad}\"/>");
    const meios::source_location dollar = refine(f, 0, 0);
    const meios::source_location inner = refine(f, 0, 2);

    REQUIRE(dollar.column == f.column_of('$'));
    REQUIRE(dollar.column > f.anchor().column);
    REQUIRE(inner.column == dollar.column + 2);
}

TEST_CASE("forward scan confirms the token column in a single-quoted value")
{
    fixture f("<a v='${x}'/>");
    const meios::source_location loc = refine(f, 0, 0);

    REQUIRE(loc.column == f.column_of('$'));
    REQUIRE(loc.line == 1);
}

TEST_CASE("forward scan skips element name and matches the target attribute by index")
{
    fixture f("<a first=\"1\" second=\"${y}\"/>");
    const meios::source_location loc = refine(f, 1, 0);

    REQUIRE(loc.column == f.column_of('$'));
}

TEST_CASE("forward scan handles a multi-line start tag")
{
    fixture f("<a\n  b=\"1\"\n  c=\"${z}\"/>");
    const meios::source_location loc = refine(f, 1, 0);

    REQUIRE(loc.column == f.column_of('$'));
    REQUIRE(loc.column > 0);
}

TEST_CASE("forward scan accounts for an entity before the token")
{
    fixture f("<a v=\"&amp;${w}\"/>");
    // Decoded value is "&${w}": index 0 is the decoded '&', index 1 the '$'.
    const meios::source_location loc = refine(f, 0, 1);

    REQUIRE(loc.column == f.column_of('$'));
}

TEST_CASE("forward scan is bounded only by the matching quote")
{
    fixture f("<a v=\"x>'y${q}\"/>");
    // '>' and the opposite quote appear inside the value; decoded index of '$' is 4.
    const meios::source_location loc = refine(f, 0, 4);

    REQUIRE(loc.column == f.column_of('$'));
}

TEST_CASE("forward scan matches duplicate attribute names positionally")
{
    fixture f("<a p=\"11\" p=\"${dup}\"/>");
    REQUIRE(f.element().attribute("p"));
    const meios::source_location loc = refine(f, 1, 0);

    REQUIRE(loc.column == f.column_of('$'));
}

TEST_CASE("forward scan handles a CRLF inside the value under wconv")
{
    fixture f("<a v=\"x\r\ny${r}\"/>");
    // wconv collapses CRLF to a single space: decoded "x y${r}", '$' at decoded index 3.
    const meios::source_location loc = refine(f, 0, 3);
    const std::size_t raw_dollar = f.text.find('$');

    REQUIRE(loc.column == f.column_at(raw_dollar));
    REQUIRE(loc.line == 2);
}

TEST_CASE("forward scan degrades to value-start when the decode diverges")
{
    // Parse a real document, then hand the helper raw text whose value bytes differ
    // (same length and tag structure) so the self-check fails and must not emit a token
    // column -- it degrades to the value-start column.
    fixture f("<a v=\"hello\"/>");
    const std::size_t value_start = f.text.find("hello");
    meios::source_location loc = meios::detail::refine_attr_column(
        "<a v=\"hELLo\"/>", f.element(), 0, 2, f.anchor());

    REQUIRE(loc.column == f.column_at(value_start));
    REQUIRE(loc.column != f.column_at(value_start + 2));
    REQUIRE(loc.line > 0);
    REQUIRE(loc.column > 0);
}

TEST_CASE("forward scan degrades to value-start when the token offset is out of range")
{
    fixture f("<a v=\"${q}\"/>");
    const std::size_t value_start = f.text.find("${q}");
    const meios::source_location loc = refine(f, 0, 999);

    REQUIRE(loc.column == f.column_at(value_start));
    REQUIRE(loc.column > 0);
}

TEST_CASE("forward scan degrades to the node anchor when the attribute is absent")
{
    fixture f("<a v=\"${q}\"/>");
    const meios::source_location loc = refine(f, 5, 0);

    REQUIRE(loc.column == f.anchor().column);
    REQUIRE(loc.line == f.anchor().line);
}

TEST_CASE("forward scan degrades to the node anchor for a synthesized node")
{
    pugi::xml_document doc;
    pugi::xml_node node = doc.append_child("x");
    node.append_attribute("v").set_value("${q}");
    const meios::source_location fallback{ "doc.xml", 7, 3 };
    const meios::source_location loc =
        meios::detail::refine_attr_column("<x v=\"${q}\"/>", node, 0, 0, fallback);

    REQUIRE(loc.line == fallback.line);
    REQUIRE(loc.column == fallback.column);
}
