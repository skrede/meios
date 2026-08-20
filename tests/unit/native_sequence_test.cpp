#include "meios/urdf/yaml_resource.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <catch2/catch_test_macros.hpp>

#ifdef MEIOS_TEST_HAS_YAML
    #include <meios/yaml/parser.h>
#endif

#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <optional>
#include <filesystem>
#include <string_view>

#ifndef MEIOS_XACRO_FIXTURE_DIR
    #error "MEIOS_XACRO_FIXTURE_DIR must name the xacro fixture root"
#endif

namespace
{

#ifdef MEIOS_TEST_HAS_YAML

std::filesystem::path fixture(std::string_view name)
{
    return std::filesystem::path{ MEIOS_XACRO_FIXTURE_DIR } / "sequence_flow" / name;
}

std::string document_text(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// The whole carrying path in one call: the project-owned auxiliary document is reached through
// the same containment seam a description uses, read by the installed parser, and the scope the
// expansion leaves behind is what the assertions read.
meios::eval_scope expand_arm(meios::log_sink &log)
{
    const std::filesystem::path document = fixture("arm.xacro");
    meios::source_stack sources;
    const std::vector<std::filesystem::path> roots;
    meios::eval_scope scope;
    scope.install_text_loader(meios::detail::make_yaml_text_loader(sources, roots, log));
    scope.install_yaml_parser(meios::make_yaml_parser());
    REQUIRE(meios::expand(document_text(document), scope, sources, document,
                          meios::expansion_limits{}, log)
                .has_value());
    return scope;
}

meios::value loaded_bounds(const meios::eval_scope &scope, meios::log_sink &log)
{
    meios::core_evaluator evaluator;
    const meios::value read = evaluator.eval("limits['shoulder_pan']['bounds']", scope, log);
    REQUIRE_FALSE(evaluator.failed());
    return read;
}

#endif

}

#ifdef MEIOS_TEST_HAS_YAML

TEST_CASE("a sequence loaded from a document crosses a property, a macro and a nested scope",
          "[native][sequence]")
{
    meios::log_sink silent;
    const meios::eval_scope scope = expand_arm(silent);
    const meios::value bounds = loaded_bounds(scope, silent);

    REQUIRE(bounds.kind() == meios::value_kind::sequence);
    REQUIRE(bounds.size() == 3);
    CHECK(scope.lookup("bounds") == std::optional<meios::value>(bounds));
    CHECK(scope.lookup("inner_bounds") == std::optional<meios::value>(bounds));
    CHECK(scope.lookup("upper") == bounds.at(std::size_t{ 2 }));
    CHECK(scope.lookup("inner_lower") == bounds.at(std::size_t{ 0 }));
}

// The auxiliary origin is carried by the loaded document itself; a member read out of it is a
// plain value until whole-tree marking lands, so the mapping is what the flag is asserted on.
TEST_CASE("the document the sequence came from reports its auxiliary origin", "[native][sequence]")
{
    meios::log_sink silent;
    const meios::eval_scope scope = expand_arm(silent);

    REQUIRE(scope.lookup("limits").has_value());
    CHECK(scope.lookup("limits")->from_yaml());
    CHECK(scope.lookup("inner_limits") == scope.lookup("limits"));
    CHECK(scope.lookup("limits")->at("elbow")->at("bounds")->size() == 2);
}

#endif
