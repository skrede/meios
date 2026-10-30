#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/source_location.h>
#include <meios/diagnostic/operation_failure.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>
#include <cstddef>

namespace
{

struct record
{
    meios::level lvl;
    meios::diagnostic_code code;
    meios::source_location loc;
    std::string message;
};

struct recorder
{
    std::vector<record> &records;

    void operator()(meios::level lvl, const std::string &message)
    {
        records.push_back({lvl, meios::diagnostic_code::unspecified, {}, message});
    }

    void operator()(meios::level lvl, const meios::source_location &loc, const std::string &message)
    {
        records.push_back({lvl, meios::diagnostic_code::unspecified, loc, message});
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &loc, const std::string &message)
    {
        records.push_back({lvl, code, loc, message});
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &loc, const meios::operation_failure &, const std::string &message)
    {
        records.push_back({lvl, code, loc, message});
    }
};

struct outcome
{
    meios::expected<meios::expansion, meios::expansion_error> result;
    std::vector<record> records;
};

outcome expand_body(const std::string &body, meios::eval_policy policy = meios::eval_policy::fail)
{
    std::vector<record> records;
    meios::log_sink_f log{recorder{records}};
    meios::source_stack sources{};
    meios::eval_scope scope;
    const std::string source = "<robot name=\"r\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">\n" + body + "</robot>";
    auto result              = meios::expand(source, scope, sources, "inline.xacro", meios::expansion_limits{}, policy, nullptr, log);
    return outcome{std::move(result), std::move(records)};
}

std::vector<record> at_level(const std::vector<record> &records, meios::level lvl)
{
    std::vector<record> kept;
    for(const record &r : records)
        if(r.lvl == lvl)
            kept.push_back(r);
    return kept;
}

std::size_t occurrences(const std::string &text, const std::string &part)
{
    std::size_t count = 0;
    for(std::size_t at = text.find(part); at != std::string::npos; at = text.find(part, at + part.size()))
        ++count;
    return count;
}

}

TEST_CASE("a refusal two deferred properties deep is one record naming both, innermost first", "[xacro][deferred]")
{
    const outcome out = expand_body("<xacro:property name=\"z\" value=\"1\"/>\n"
                                    "<xacro:property name=\"q\" value=\"${nope}\"/>\n"
                                    "<xacro:property name=\"p\" value=\"a${q}\"/>\n"
                                    "<link name=\"${p}\"/>\n");
    REQUIRE_FALSE(out.result.has_value());
    const std::vector<record> errors = at_level(out.records, meios::level::error);
    REQUIRE(errors.size() == 1);
    const record &only = errors.front();
    CHECK(only.code == meios::diagnostic_code::undefined_property);
    CHECK(only.loc.line == 3);
    const std::size_t inner = only.message.find("when evaluating 'q' (defined at inline.xacro:3), read at inline.xacro:4");
    const std::size_t outer = only.message.find("when evaluating 'p' (defined at inline.xacro:4), read at inline.xacro:5");
    REQUIRE(inner != std::string::npos);
    REQUIRE(outer != std::string::npos);
    CHECK(inner < outer);
    CHECK(out.result.error().code == only.code);
    CHECK(out.result.error().loc.line == only.loc.line);
    CHECK(out.result.error().message == only.message);
}

TEST_CASE("a refusal one deferred property deep names that property and where it was read", "[xacro][deferred]")
{
    const outcome out = expand_body("<xacro:property name=\"p\" value=\"a${nope}\"/>\n"
                                    "<link name=\"x${p}\"/>\n");
    REQUIRE_FALSE(out.result.has_value());
    const std::vector<record> errors = at_level(out.records, meios::level::error);
    REQUIRE(errors.size() == 1);
    CHECK(errors.front().code == meios::diagnostic_code::undefined_property);
    CHECK(errors.front().loc.line == 2);
    CHECK(occurrences(errors.front().message, "when evaluating") == 1);
    CHECK(errors.front().message.find("when evaluating 'p' (defined at inline.xacro:2), read at inline.xacro:3") != std::string::npos);
    CHECK(out.result.error().message == errors.front().message);
}

TEST_CASE("a warning raised while resolving a deferred property reaches the caller once", "[xacro][deferred]")
{
    const outcome out = expand_body("<xacro:property name=\"p\" value=\"${[1, 2]}\"/>\n"
                                    "<link name=\"x${p}${p}\"/>\n",
                                    meios::eval_policy::warn);
    REQUIRE(out.result.has_value());
    const std::vector<record> warnings = at_level(out.records, meios::level::warn);
    REQUIRE(warnings.size() == 1);
    CHECK(warnings.front().code == meios::diagnostic_code::unsupported_expression);
    CHECK(at_level(out.records, meios::level::error).empty());
}

TEST_CASE("a refused deferred property read in a macro call's argument names the call as the read site", "[xacro][deferred]")
{
    const outcome out = expand_body("<xacro:property name=\"p\" value=\"a${nope}\"/>\n"
                                    "<xacro:macro name=\"m\" params=\"v\"><link name=\"${v}\"/></xacro:macro>\n"
                                    "<xacro:m v=\"${p}\"/>\n");
    REQUIRE_FALSE(out.result.has_value());
    const std::vector<record> errors = at_level(out.records, meios::level::error);
    REQUIRE(errors.size() == 1);
    CHECK(errors.front().code == meios::diagnostic_code::undefined_property);
    CHECK(errors.front().loc.line == 2);
    CHECK(errors.front().message.find("when evaluating 'p' (defined at inline.xacro:2), read at inline.xacro:4") != std::string::npos);
}
