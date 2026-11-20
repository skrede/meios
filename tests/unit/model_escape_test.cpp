#include <meios/detail/escaped_controls.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/source_location.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>
#include <sstream>
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

}

TEST_CASE("an ASCII control byte is written visibly and every other ASCII byte as it is", "[model][escape]")
{
    CHECK(escaped_controls("\ta\r\n") == R"(\ta\r\n)");
    CHECK(escaped_controls(std::string_view("\x00\x01\x1f\x7f", 4)) == R"(\x00\x01\x1f\x7f)");
    CHECK(escaped_controls(" a\\b \xc3\xa9") == " a\\b \xc3\xa9");
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

TEST_CASE("a character beside the escaped classes and a byte that begins no sequence are copied", "[model][escape]")
{
    CHECK(escaped_controls("\xc2\xa0") == "\xc2\xa0");
    CHECK(escaped_controls("\xe2\x80\xa7") == "\xe2\x80\xa7");
    CHECK(escaped_controls("\x85") == "\x85");
    CHECK(escaped_controls("\xe2\x80") == "\xe2\x80");
    CHECK(escaped_controls("\xc0\x8a") == "\xc0\x8a");
}

TEST_CASE("escaping escaped text changes nothing", "[model][escape]")
{
    const std::string sample = "\t\r\n\0\x1f\x7f a\\b \xc3\xa9 \xc2\x85\xc2\x9b \xe2\x80\xa8\xe2\x80\xa9 \xc2\xa0\xe2\x80\xa7 \x85\xe2\x80"s;
    const std::string once   = escaped_controls(sample);
    CHECK(escaped_controls(once) == once);
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
