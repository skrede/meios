#include "meios/detail/utf8.h"
#include "meios/xacro/unicode_data.h"
#include "meios/xacro/identifier_text.h"
#include "meios/xacro/argument_conversion.h"

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <string>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <optional>
#include <algorithm>
#include <string_view>

namespace
{

std::string spelled(std::u32string_view points)
{
    std::string out;
    for(char32_t point : points)
        meios::detail::append_utf8(out, point);
    return out;
}

template<typename Row>
bool ordered(std::span<const Row> rows)
{
    for(std::size_t k = 0; k < rows.size(); ++k)
        if(rows[k].first > rows[k].last || (k + 1 < rows.size() && rows[k].last >= rows[k + 1].first))
            return false;
    return !rows.empty();
}

}

using meios::detail::python_name;
using meios::detail::valid_python_name;

TEST_CASE("an ASCII identifier answers its own spelling", "[xacro][identifier]")
{
    for(const char *name : {"a", "_", "a1", "__x", "match", "if", "True"})
        REQUIRE(python_name(name) == std::optional<std::string>(name));
}

TEST_CASE("an identifier outside ASCII answers the NFKC form Python stores", "[xacro][identifier]")
{
    const std::pair<std::u32string_view, std::u32string_view> forms[] = {{U"π", U"π"},
                                                                         {U"ﬁ", U"fi"},
                                                                         {U"ａ", U"a"},
                                                                         {U"x́", U"x́"},
                                                                         {U"é", U"é"},
                                                                         {U"Ω", U"Ω"},
                                                                         {U"a·", U"a·"},
                                                                         {U"a１", U"a1"},
                                                                         {U"_＿x", U"__x"},
                                                                         {U"가", U"가"},
                                                                         {U"\U00016D63\U00016D67\U00016D67", U"\U00016D6A"}};
    for(const auto &[spelling, form] : forms)
        REQUIRE(python_name(spelled(spelling)) == std::optional<std::string>(spelled(form)));
}

TEST_CASE("a text that is no Python identifier answers nothing", "[xacro][identifier]")
{
    for(std::u32string_view spelling : {U"·a", U"a€", U"１", U"＿x", U""})
        REQUIRE_FALSE(python_name(spelled(spelling)));
}

TEST_CASE("a malformed UTF-8 sequence never forms a name", "[xacro][identifier]")
{
    for(std::string_view bytes : {"\x80", "\xC1\x81", "\xC0\xB1", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xCF", "a\xCF", "a\x80"})
        REQUIRE_FALSE(python_name(bytes));
}

TEST_CASE("a name is valid exactly as the reference's is_valid_name admits it", "[xacro][identifier]")
{
    for(std::u32string_view name : {U"π", U"é", U"match", U"_", U"fi"})
        REQUIRE(valid_python_name(spelled(name)));
    for(std::u32string_view name : {U"ﬁ", U"if", U"True", U"é", U"Ω", U""})
        REQUIRE_FALSE(valid_python_name(spelled(name)));
}

TEST_CASE("a long run of combining marks is put in canonical order, each class in its written order", "[xacro][identifier]")
{
    const std::size_t pairs = 25000;
    std::u32string marks;
    for(std::size_t k = 0; k < pairs; ++k)
        marks += U"̖́";
    const std::u32string ordered_marks = std::u32string(pairs, U'̖') + std::u32string(pairs, U'́');
    REQUIRE(python_name(spelled(U"x" + marks)) == std::optional<std::string>(spelled(U"x" + ordered_marks)));
    REQUIRE(python_name(spelled(U"a" + marks)) == std::optional<std::string>(spelled(U"á" + ordered_marks.substr(0, 2 * pairs - 1))));
}

TEST_CASE("the identifier and normalization tables are sorted and hold the counts the generator wrote", "[xacro][identifier]")
{
    REQUIRE(ordered(meios::detail::xid_start_ranges()));
    REQUIRE(ordered(meios::detail::xid_continue_ranges()));
    REQUIRE(ordered(meios::detail::combining_runs()));
    REQUIRE(meios::detail::xid_start_ranges().size() == 682);
    REQUIRE(meios::detail::xid_continue_ranges().size() == 392);
    REQUIRE(meios::detail::combining_runs().size() == 393);
    REQUIRE(meios::detail::compositions().size() == 961);
    REQUIRE(meios::detail::decompositions().size() == 4546);
    REQUIRE(meios::detail::decomposition_pool().size() == 6586);
}

TEST_CASE("the compositions are sorted by pair and every decomposition lies within the pool", "[xacro][identifier]")
{
    const auto pair = [](const meios::detail::composition &one) { return std::pair(one.first, one.second); };
    REQUIRE(std::ranges::is_sorted(meios::detail::compositions(), std::ranges::less{}, pair));
    REQUIRE(std::ranges::is_sorted(meios::detail::decompositions(), std::ranges::less{}, &meios::detail::decomposition::code_point));
    for(const meios::detail::decomposition &one : meios::detail::decompositions())
        REQUIRE(std::size_t{one.offset} + one.length <= meios::detail::decomposition_pool().size());
}

TEST_CASE("an int() argument decodes strictly, so an overlong digit stays text", "[xacro][identifier]")
{
    const std::optional<meios::value> overlong = meios::detail::converted_argument("\xC0\xB1");
    REQUIRE(overlong);
    REQUIRE(overlong->text() == std::optional<std::string>("\xC0\xB1"));
    const std::optional<meios::value> arabic = meios::detail::converted_argument(spelled(U"١٢"));
    REQUIRE(arabic);
    REQUIRE(arabic->integer() == std::optional<std::int64_t>(12));
}
