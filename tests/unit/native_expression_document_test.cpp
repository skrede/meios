#include "fake_yaml_parser.h"
#include "native_expression_fixture.h"

#include "meios/xacro/eval_session.h"
#include "meios/xacro/substitution_detail.h"

#include "meios/urdf/yaml_resource.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <filesystem>
#include <functional>
#include <string_view>

#ifndef MEIOS_URDF_FIXTURE_DIR
    #error "MEIOS_URDF_FIXTURE_DIR must name the urdf fixture root"
#endif

using native_expression_test::evaluate;
using native_expression_test::recorder;

namespace
{

std::string document_text(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// The fixture is expanded rather than loaded, because the shape under test is the composed
// attribute text itself, before any reader gives it a meaning.
std::string expanded(std::string_view name, meios::log_sink &log)
{
    const std::filesystem::path document = std::filesystem::path{MEIOS_URDF_FIXTURE_DIR} / "native_expr" / name;
    meios::source_stack sources;
    const std::vector<std::filesystem::path> roots;
    meios::eval_scope scope;
    scope.install_text_loader(meios::detail::make_yaml_text_loader(sources, roots, log));
    scope.install_yaml_parser(fake::yaml_parser_handle());
    const meios::expected<meios::expansion, meios::expansion_error> out = meios::expand(document_text(document), scope, sources, document, meios::expansion_limits{}, log);
    return out ? out->document : std::string();
}

bool substitution_survives(meios::eval_policy policy, std::size_t token_ceiling)
{
    const meios::evaluator_limits ceilings{0, 0, 0, token_ceiling, 0};
    meios::detail::eval_session session(ceilings);
    meios::eval_scope scope;
    meios::source_stack sources;
    meios::log_sink silent;
    return meios::detail::substitute_refined("value ${1 + 1}", scope, sources, "inline.xacro", policy, {}, session, silent, {}).has_value();
}

}

TEST_CASE("the namespaced document call reaches bytes only through the scope", "[native][expression]")
{
    CHECK(evaluate("xacro.load_yaml(seed_file)['mesh_files'][name][type]['mesh']['package']").rendered == "ur_description");

    meios::eval_scope bare;
    bare.set("seed_file", meios::value{std::string("seed.yaml")});
    recorder heard;
    meios::log_sink_f sink{std::ref(heard)};
    meios::core_evaluator evaluator;
    evaluator.eval("xacro.load_yaml(seed_file)", bare, sink);

    CHECK(evaluator.failed());
    REQUIRE_FALSE(heard.messages.empty());
    CHECK(heard.messages.front().find("auxiliary document") != std::string::npos);
}

TEST_CASE("a mapping crosses a property, a macro argument and a nested scope and still reads", "[native][expression]")
{
    meios::log_sink log;
    const std::string out = expanded("subscripts.urdf.xacro", log);

    REQUIRE_FALSE(out.empty());
    CHECK(out.find("filename=\"package://ur_description/meshes/base.dae\"") != std::string::npos);
    CHECK(out.find("<link name=\"visual\"") != std::string::npos);
}

TEST_CASE("a boolean read out of an auxiliary document drives a conditional", "[native][expression]")
{
    meios::log_sink log;
    const std::string out = expanded("subscripts.urdf.xacro", log);

    REQUIRE_FALSE(out.empty());
    CHECK(out.find("type=\"continuous\"") != std::string::npos);
    CHECK(out.find("type=\"revolute\"") == std::string::npos);
}

TEST_CASE("an expression over the token ceiling halts under every evaluation policy", "[native][expression]")
{
    REQUIRE(substitution_survives(meios::eval_policy::fail, 64));
    REQUIRE(substitution_survives(meios::eval_policy::warn, 64));
    REQUIRE(substitution_survives(meios::eval_policy::skip, 64));

    REQUIRE_FALSE(substitution_survives(meios::eval_policy::fail, 1));
    REQUIRE_FALSE(substitution_survives(meios::eval_policy::warn, 1));
    REQUIRE_FALSE(substitution_survives(meios::eval_policy::skip, 1));
}
