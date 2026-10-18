#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstdint>

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;

struct located
{
    meios::diagnostic_code code;
    meios::source_location loc;
};

struct recorder
{
    std::vector<located> &sink;

    void operator()(meios::level, const std::string &)
    {
    }

    void operator()(meios::level, const meios::source_location &, const std::string &)
    {
    }

    void operator()(meios::level, meios::diagnostic_code code, const meios::source_location &loc, const std::string &)
    {
        sink.push_back({code, loc});
    }
};

expansion_result run(const std::string &source, std::vector<located> &records)
{
    meios::log_sink_f log{recorder{records}};
    meios::source_stack sources;
    meios::eval_scope scope;
    return meios::expand(source, scope, sources, "robot.xacro", meios::expansion_limits{}, log);
}

std::string document(const std::string &offending)
{
    return "<robot xmlns:xacro=\"http://www.ros.org/wiki/xacro\">\n"
           "  <link name=\"base\"/>\n"
           "\n" +
            offending + "\n</robot>\n";
}

void require_located_at(const std::string &offending, std::int32_t line, meios::diagnostic_code code = meios::diagnostic_code::unsupported_expression)
{
    std::vector<located> records;
    const expansion_result out = run(document(offending), records);
    INFO("offending: " << offending);

    REQUIRE_FALSE(out.has_value());
    CHECK(out.error().code == code);
    CHECK(out.error().loc.file.filename() == "robot.xacro");
    CHECK(out.error().loc.line == line);

    REQUIRE_FALSE(records.empty());
    CHECK(records.front().code == code);
    CHECK(records.front().loc.file.filename() == "robot.xacro");
    CHECK(records.front().loc.line == line);
}

}

TEST_CASE("an unsupported form in an attribute refuses at the line of its element", "[xacro][unsupported][location]")
{
    require_located_at("  <link name=\"${tuple(1)}\"/>", 4);
    require_located_at("  <link name=\"${list(1)}\"/>", 4);
}

TEST_CASE("an unsupported form in a property value refuses at the line of the property", "[xacro][unsupported][location]")
{
    require_located_at("  <xacro:property name=\"p\" value=\"${dict([('a', 1)])}\"/>\n"
                       "  <link name=\"${p}\"/>",
                       4);
}

TEST_CASE("an unsupported form in element text refuses at the line it sits on", "[xacro][unsupported][location]")
{
    require_located_at("  <link name=\"base2\">\n    <a>${set(1)}</a>\n  </link>", 5);
}

TEST_CASE("a slice or a double-quoted string refuses as unsupported at the line it sits on", "[xacro][unsupported][location]")
{
    require_located_at("  <link name=\"${'abc'[0:2]}\"/>", 4);
    require_located_at("  <link name='${\"abc\"}'/>", 4);
    require_located_at("  <xacro:property name=\"p\" value=\"${'abc'[1:]}\"/>\n  <link name=\"${p}\"/>", 4);
}

// A comprehension is not recognized as a construct at all, so it refuses as an expression the
// evaluator cannot read rather than as a named unsupported form; the refusal must still be located.
TEST_CASE("a comprehension refuses as an expression error at the line it sits on", "[xacro][unsupported][location]")
{
    const meios::diagnostic_code refused = meios::diagnostic_code::expression_error;
    require_located_at("  <link name=\"${[i for i in [1, 2]]}\"/>", 4, refused);
    require_located_at("  <link name=\"base2\">\n    <a>${[c for c in 'ab']}</a>\n  </link>", 5, refused);
}
