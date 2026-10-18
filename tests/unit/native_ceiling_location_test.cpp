#include "error_sink.h"
#include "fake_yaml_parser.h"
#include "native_yaml_seam_fixture.h"

#include <meios/io/source_stack.h>
#include <meios/io/memory_source.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <filesystem>
#include <functional>
#include <string_view>

namespace
{

using error_sink_test::error_sink;
using error_sink_test::ends_on_one_record;
using error_sink_test::every_record_names;
using substituted = meios::expected<meios::substitution, meios::expansion_error>;

const meios::source_location span_at{"robot.xacro", 7, 3};

constexpr meios::eval_policy every_policy[] = {meios::eval_policy::fail, meios::eval_policy::warn, meios::eval_policy::skip};

substituted under_steps(std::string_view raw, meios::eval_policy policy, std::size_t steps, meios::log_sink &heard)
{
    const meios::evaluator_limits ceilings{0, 0, 0, 0, steps};
    meios::detail::eval_session session(ceilings);
    meios::eval_scope scope;
    meios::source_stack sources;
    return meios::detail::substitute_refined(raw, scope, sources, "robot.xacro", policy, {}, session, heard, span_at);
}

substituted under_bytes(meios::eval_policy policy, meios::log_sink &heard)
{
    std::size_t delivered = 0;
    yaml_seam_test::generating_operations operations(yaml_seam_test::generated_size, delivered);
    meios::memory_source memory{heard};
    memory.add("pkg", "cfg.yaml", "value: 9");
    meios::source_stack sources{std::move(memory)};
    const std::vector<std::filesystem::path> roots;
    yaml_seam_test::null_probe probe;
    meios::eval_scope scope;
    scope.install_text_loader(meios::detail::make_yaml_text_loader(sources, roots, heard, operations, probe, yaml_seam_test::read_ceiling));
    scope.install_yaml_parser(fake::yaml_parser_handle());
    const meios::evaluator_limits ceilings;
    meios::detail::eval_session session(ceilings);
    return meios::detail::substitute_refined("v ${xacro.load_yaml('package://pkg/cfg.yaml')['value']}", scope, sources, "robot.xacro", policy, {}, session, heard, span_at);
}

}

TEST_CASE("a crossed step ceiling is located, classified as the budget, and terminal under every policy", "[native][budget]")
{
    const std::string nest = "value ${((((((((1))))))))}";
    for(meios::eval_policy policy : every_policy)
    {
        error_sink admitted;
        CHECK(under_steps(nest, policy, 4096, admitted).has_value());
        CHECK(admitted.errors.empty());
        error_sink heard;
        const substituted out = under_steps(nest, policy, 64, heard);
        ends_on_one_record(out, heard, meios::diagnostic_code::expansion_budget_exceeded, span_at);
    }
}

TEST_CASE("a crossed byte ceiling is located, classified as the budget, and terminal under every policy", "[native][yaml]")
{
    for(meios::eval_policy policy : every_policy)
    {
        INFO("policy " << static_cast<int>(policy));
        error_sink heard;
        const substituted out = under_bytes(policy, heard);
        ends_on_one_record(out, heard, meios::diagnostic_code::expansion_budget_exceeded, span_at);
        every_record_names(heard, "\"package://pkg/cfg.yaml\"");
        every_record_names(heard, "loader's read ceiling");
    }
}
