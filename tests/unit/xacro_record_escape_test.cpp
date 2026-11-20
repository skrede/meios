#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <sstream>

namespace
{

const std::string head = "<robot name=\"p\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">\n";
const std::string forged = "a&#10;[error] forged";
const std::string defines_t = "<xacro:property name=\"t\" value=\"" + forged + "\"/>";

std::string written_refusal(const std::string &body)
{
    std::ostringstream out;
    meios::log_sink_s log{out};
    meios::source_stack sources{};
    meios::eval_scope scope;
    const auto result = meios::expand(head + body + "</robot>", scope, sources, "inline.xacro", meios::expansion_limits{}, log);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().code == meios::diagnostic_code::xacro_structural_error);
    return out.str();
}

std::vector<std::string> lines_of(const std::string &text)
{
    std::vector<std::string> lines;
    std::istringstream in(text);
    for(std::string line; std::getline(in, line);)
        lines.push_back(line);
    return lines;
}

std::string one_error_line(const std::string &body)
{
    std::vector<std::string> errors;
    for(const std::string &line : lines_of(written_refusal(body)))
    {
        REQUIRE_FALSE(line.starts_with("[error]"));
        if(line.find("[error]") != std::string::npos)
            errors.push_back(line);
    }
    REQUIRE(errors.size() == 1);
    REQUIRE(errors.front().starts_with("inline.xacro:"));
    return errors.front();
}

std::vector<std::string> bodies_quoting(const std::string &text)
{
    return {
            "<xacro:call macro=\"" + text + "\"/>",
            "<xacro:if value=\"" + text + "\"/>",
            "<xacro:unless value=\"" + text + "\"/>",
            "<xacro:include filename=\"x.xacro\" optional=\"" + text + "\"/>",
            "<xacro:property name=\"p\" value=\"1\" remove=\"" + text + "\"/>",
            "<xacro:property name=\"p\" value=\"1\" lazy_eval=\"" + text + "\"/>",
    };
}

}

TEST_CASE("a refusal quoting authored text with a line feed is written as one record", "[xacro][records]")
{
    for(const std::string &body : bodies_quoting(forged))
    {
        INFO(body);
        REQUIRE(one_error_line(body).find(R"(a\n[error] forged)") != std::string::npos);
    }
}

TEST_CASE("a refusal quoting evaluated text with a line feed is written as one record", "[xacro][records]")
{
    for(const std::string &body : bodies_quoting("${t}"))
    {
        INFO(body);
        REQUIRE(one_error_line(defines_t + body).find(R"(a\n[error] forged)") != std::string::npos);
    }
}

TEST_CASE("a refusal quoting a NEL or a line separator writes it as a code point", "[xacro][records]")
{
    const std::string nel = one_error_line("<xacro:if value=\"a&#133;b\"/>");
    REQUIRE(nel.find(R"(a\u0085b)") != std::string::npos);
    const std::string separator = one_error_line("<xacro:call macro=\"a&#x2028;b\"/>");
    REQUIRE(separator.find(R"(a\u2028b)") != std::string::npos);
    for(const std::string &body : {std::string("<xacro:if value=\"a&#133;b\"/>"), std::string("<xacro:call macro=\"a&#x2028;b\"/>")})
    {
        INFO(body);
        const std::string stream = written_refusal(body);
        REQUIRE(stream.find("\xc2\x85") == std::string::npos);
        REQUIRE(stream.find("\xe2\x80\xa8") == std::string::npos);
    }
}
