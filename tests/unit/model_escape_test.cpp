#include <meios/detail/utf8.h>
#include <meios/detail/escaped_controls.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/source_location.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <charconv>
#include <stdexcept>
#include <string_view>

using meios::detail::escaped_controls;
using namespace std::string_literals;

namespace
{

std::size_t line_count(const std::string &text)
{
    std::size_t lines = 0;
    for(const char one : text)
        lines += one == '\n' ? 1 : 0;
    return lines;
}

char32_t hex_value(std::string_view digits, std::size_t count)
{
    std::uint32_t value     = 0;
    const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), value, 16);
    if(digits.size() != count || error != std::errc{} || end != digits.data() + digits.size())
        throw std::invalid_argument("a truncated or malformed hex escape");
    return value;
}

std::string unescaped(std::string_view text)
{
    std::string out;
    for(std::size_t at = 0; at < text.size(); ++at)
    {
        if(text[at] != '\\')
        {
            out += text[at];
            continue;
        }
        const char kind = text.at(++at);
        if(kind == 'x')
            out += static_cast<char>(hex_value(text.substr(at + 1, 2), 2));
        else if(kind == 'u')
            meios::detail::append_utf8(out, hex_value(text.substr(at + 1, 4), 4));
        else if(kind != '\\' && kind != 't' && kind != 'r' && kind != 'n')
            throw std::invalid_argument("an escape this escaping never writes");
        else
            out += kind == 't' ? '\t' : kind == 'r' ? '\r' : kind == 'n' ? '\n' : '\\';
        at += kind == 'x' ? 2 : kind == 'u' ? 4 : 0;
    }
    return out;
}

bool raw_control(char32_t point)
{
    return point < 0x20 || point == 0x7F || (point >= 0x80 && point <= 0x9F) || point == 0x2028 || point == 0x2029;
}

bool holds_raw_control(std::string_view text)
{
    for(std::size_t at = 0; at < text.size();)
    {
        const meios::detail::utf8_point point = meios::detail::decode_utf8(text, at);
        const auto byte                       = static_cast<std::uint8_t>(text[at]);
        if(point.value == meios::detail::malformed_utf8 ? byte >= 0x80 && byte <= 0x9F : raw_control(point.value))
            return true;
        at += point.length;
    }
    return false;
}

bool reads_back(const std::string &text)
{
    const std::string out = escaped_controls(text);
    try
    {
        return unescaped(out) == text && !holds_raw_control(out);
    }
    catch(const std::exception &)
    {
        return false;
    }
}

std::vector<std::string> short_strings()
{
    std::vector<std::string> all;
    for(std::uint32_t first = 0; first < 256; ++first)
    {
        all.push_back(std::string(1, static_cast<char>(first)));
        all.push_back("\xe2\x80"s + static_cast<char>(first));
        for(std::uint32_t second = 0; second < 256; ++second)
            all.push_back(std::string{static_cast<char>(first), static_cast<char>(second)});
    }
    return all;
}

}

TEST_CASE("an ASCII control byte and a backslash are written visibly and every other ASCII byte as it is", "[model][escape]")
{
    CHECK(escaped_controls("\ta\r\n") == R"(\ta\r\n)");
    CHECK(escaped_controls(std::string_view("\x00\x01\x1f\x7f", 4)) == R"(\x00\x01\x1f\x7f)");
    CHECK(escaped_controls("a\\b") == R"(a\\b)");
    CHECK(escaped_controls(" a\\b \xc3\xa9") == " a\\\\b \xc3\xa9");
    CHECK(escaped_controls("\\n") == R"(\\n)");
    CHECK(escaped_controls("").empty());
}

TEST_CASE("a C1 control and the Unicode line and paragraph separators are written as code points", "[model][escape]")
{
    CHECK(escaped_controls("a\xc2\x85z") == R"(a\u0085z)");
    CHECK(escaped_controls("\xc2\x80") == R"(\u0080)");
    CHECK(escaped_controls("\xc2\x9b") == R"(\u009b)");
    CHECK(escaped_controls("\xc2\x9f") == R"(\u009f)");
    CHECK(escaped_controls("\xe2\x80\xa8") == R"(\u2028)");
    CHECK(escaped_controls("\xe2\x80\xa9") == R"(\u2029)");
}

TEST_CASE("a character beside the escaped classes is copied", "[model][escape]")
{
    CHECK(escaped_controls("\xc2\xa0") == "\xc2\xa0");
    CHECK(escaped_controls("\xe2\x80\xa7") == "\xe2\x80\xa7");
}

TEST_CASE("a byte that begins no sequence is written as a hex byte in 0x80..0x9F and copied above it", "[model][escape]")
{
    CHECK(escaped_controls("\x9b") == R"(\x9b)");
    CHECK(escaped_controls("\x80") == R"(\x80)");
    CHECK(escaped_controls("\x9f") == R"(\x9f)");
    CHECK(escaped_controls("\x85") == R"(\x85)");
    CHECK(escaped_controls("\xa0") == "\xa0");
    CHECK(escaped_controls("\xff") == "\xff");
    CHECK(escaped_controls("\xc2") == "\xc2");
    CHECK(escaped_controls("\xe2\x80") == "\xe2"s + R"(\x80)");
    CHECK(escaped_controls("\xc0\x8a") == "\xc0"s + R"(\x8a)");
}

TEST_CASE("escaped text reads back to the text raised and holds no raw control or stray C1 byte", "[model][escape]")
{
    std::vector<std::string> failed;
    for(const std::string &text : short_strings())
        if(!reads_back(text))
            failed.push_back(text);
    CHECK(failed.empty());
    const std::string sample = "\t\r\n\0\x1f\x7f a\\b \\x41 \\u0041 \xc3\xa9 \xc2\x85\xc2\x9b \xe2\x80\xa8\xe2\x80\xa9 \xc2\xa0\xe2\x80\xa7 \x85\x9b\xa0\xff\xe2\x80"s;
    CHECK(reads_back(sample));
    CHECK(escaped_controls(sample) != sample);
}

TEST_CASE("the stream sink writes a located record on one line whatever its message holds", "[model][escape]")
{
    std::ostringstream out;
    meios::log_sink_s adapter{out};
    meios::log_sink &seam = adapter;
    seam.log(meios::level::error, meios::diagnostic_code::xacro_structural_error, meios::source_location{"a.xml", 3, 7}, "a\n[error] forged");
    REQUIRE(out.str() == "a.xml:3:7: [error] (xacro_structural_error) a\\n[error] forged\n");
}

TEST_CASE("the stream sink writes an unlocated record and a record with a broken location on one line", "[model][escape]")
{
    std::ostringstream out;
    meios::log_sink_s adapter{out};
    meios::log_sink &seam = adapter;
    seam.log(meios::level::warn, "a\xc2\x85z\xe2\x80\xa8z");
    REQUIRE(out.str() == "[warn] a\\u0085z\\u2028z\n");
    out.str({});
    seam.log(meios::level::error, meios::source_location{"a\nb.xml", 1, 1}, "located");
    REQUIRE(line_count(out.str()) == 1);
    REQUIRE(out.str().find("a\\nb.xml") != std::string::npos);
}
