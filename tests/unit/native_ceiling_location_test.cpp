#include "fake_yaml_parser.h"
#include "native_yaml_seam_fixture.h"

#include <meios/io/source_stack.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <cstddef>
#include <functional>
#include <string_view>

namespace
{

using substituted = meios::expected<meios::substitution, meios::expansion_error>;

struct record
{
    meios::diagnostic_code code;
    meios::source_location loc;
};

struct located_sink final : public meios::log_sink
{
    using meios::log_sink::log;

    std::vector<record> records;

    void log(meios::level lvl, meios::diagnostic_code code, const meios::source_location &loc, const std::string &) override
    {
        if(lvl == meios::level::error)
            records.push_back({code, loc});
    }
};

const meios::source_location span_at{"robot.xacro", 7, 3};

constexpr meios::eval_policy every_policy[] = {meios::eval_policy::fail, meios::eval_policy::warn, meios::eval_policy::skip};

void located_on_budget(const substituted &out, const located_sink &heard)
{
    REQUIRE_FALSE(out.has_value());
    CHECK(out.error().code == meios::diagnostic_code::expansion_budget_exceeded);
    CHECK(out.error().loc.file == span_at.file);
    CHECK(out.error().loc.line == span_at.line);
    CHECK(heard.records.size() == 1);
    for(const record &one : heard.records)
    {
        INFO("recorded " << meios::to_string(one.code) << " at " << meios::to_string(one.loc));
        CHECK(one.code == meios::diagnostic_code::expansion_budget_exceeded);
        CHECK(one.loc.file == span_at.file);
        CHECK(one.loc.line == span_at.line);
    }
}

substituted under_steps(std::string_view raw, meios::eval_policy policy, std::size_t steps, located_sink &heard)
{
    const meios::evaluator_limits ceilings{0, 0, 0, 0, steps};
    meios::detail::eval_session session(ceilings);
    meios::eval_scope scope;
    meios::source_stack sources;
    return meios::detail::substitute_refined(raw, scope, sources, "robot.xacro", policy, {}, session, heard, span_at);
}

}

TEST_CASE("a crossed step ceiling is located, classified as the budget, and terminal under every policy", "[native][budget]")
{
    const std::string nest = "value ${((((((((1))))))))}";
    for(meios::eval_policy policy : every_policy)
    {
        located_sink admitted;
        CHECK(under_steps(nest, policy, 4096, admitted).has_value());
        CHECK(admitted.records.empty());
        located_sink heard;
        located_on_budget(under_steps(nest, policy, 64, heard), heard);
    }
}
