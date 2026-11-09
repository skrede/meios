#include "eval_python_fixture.h"

#include <meios/diagnostic/claims.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <functional>
#include <string_view>

namespace
{

// The shared fixture's tally counts errors only, so a case about a warning needs a sink that
// keeps every level and whether the record arrived with a position.
struct journal
{
    struct record
    {
        meios::level lvl;
        std::string where;
        std::string message;
        meios::diagnostic_code code;
    };

    std::vector<record> records;

    void operator()(meios::level lvl, const std::string &message)
    {
        records.push_back({ lvl, std::string{}, message, meios::diagnostic_code::unspecified });
    }

    void operator()(meios::level lvl, const meios::source_location &at, const std::string &message)
    {
        records.push_back({ lvl, meios::to_string(at), message, meios::diagnostic_code::unspecified });
    }

    // Without this overload log_sink_f degrades a coded record to the located one, so the code a
    // relocating sink synthesizes is invisible and nothing can assert it.
    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &at,
                    const std::string &message)
    {
        records.push_back({ lvl, meios::to_string(at), message, code });
    }
};

struct journaled
{
    expansion_result expanded;
    std::vector<journal::record> records;
};

journaled expand_journalled(std::string_view document, meios::eval_policy policy,
                            std::optional<std::string> yaml = std::nullopt)
{
    journal recorded;
    meios::log_sink_f sink{ std::ref(recorded) };
    meios::eval_scope scope;
    if(yaml)
        install_yaml(scope, std::move(*yaml));
    meios::source_stack sources;
    const auto handle = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
    expansion_result out = meios::expand(document, scope, sources, "robot.xacro",
                                         meios::expansion_limits{}, policy, handle, sink);
    return journaled{ std::move(out), std::move(recorded.records) };
}

std::vector<journal::record> warnings(const journaled &result)
{
    std::vector<journal::record> picked;
    for(const journal::record &one : result.records)
    {
        if(one.lvl == meios::level::warn)
            picked.push_back(one);
    }
    return picked;
}

}

TEST_CASE("a null result serializes as None and says so at a position", "[eval_python]")
{
    const journaled resolved = expand_journalled(span_document("${None}"), meios::eval_policy::fail);
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(resolved.expanded->document.find("<l>None</l>") != std::string::npos);
    const std::vector<journal::record> warned = warnings(resolved);
    REQUIRE(warned.size() == 1);
    REQUIRE(warned.front().message.find("the expression result is null") != std::string::npos);
    REQUIRE(warned.front().where.find("robot.xacro:1:") != std::string::npos);
}

// claims_from clears a completeness bit per record code without regard to level, so a warn typed
// as an expression failure retracts the parse claim on a load that succeeded.
TEST_CASE("a null result warns without claiming the expression failed", "[eval_python]")
{
    const journaled resolved = expand_journalled(span_document("${None}"), meios::eval_policy::fail);
    REQUIRE(resolved.expanded.has_value());
    const std::vector<journal::record> warned = warnings(resolved);
    REQUIRE(warned.size() == 1);
    CHECK(warned.front().code != meios::diagnostic_code::expression_error);
    CHECK(meios::cleared_by(warned.front().code) == meios::completeness::none);
}

TEST_CASE("a yaml key written with no value flattens as None under every policy", "[eval_python]")
{
    const meios::eval_policy policies[] = { meios::eval_policy::fail, meios::eval_policy::warn,
                                            meios::eval_policy::skip };
    for(meios::eval_policy policy : policies)
    {
        const journaled resolved = expand_journalled(yaml_document("${config['empty']}"), policy,
                                                     "empty:\n");
        REQUIRE(resolved.expanded.has_value());
        CHECK(resolved.expanded->document.find("<l>None</l>") != std::string::npos);
        const std::vector<journal::record> warned = warnings(resolved);
        REQUIRE(warned.size() == 1);
        CHECK(warned.front().message.find("the expression result is null") != std::string::npos);
        CHECK(warned.front().where.find("robot.xacro:1:") != std::string::npos);
    }
}

TEST_CASE("a string spelled None is a string and reports nothing", "[eval_python]")
{
    const journaled resolved = expand_journalled(span_document("${'None'}"), meios::eval_policy::fail);
    REQUIRE(resolved.expanded.has_value());
    REQUIRE(resolved.expanded->document.find("<l>None</l>") != std::string::npos);
    REQUIRE(resolved.records.empty());
}