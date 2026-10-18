#include "fake_yaml_parser.h"
#include "native_member_fixture.h"

#include "meios/urdf/yaml_resource.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <iterator>
#include <filesystem>
#include <string_view>

#ifndef MEIOS_URDF_FIXTURE_DIR
    #error "MEIOS_URDF_FIXTURE_DIR must name the urdf fixture root"
#endif

using native_member_test::evaluate;
using native_member_test::outcome;

namespace
{

std::string document_text(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// Expanded rather than loaded, because the shape under test is the composed attribute text
// itself, carried across a property, a macro argument and the macro's own nested scope.
std::string expanded(std::string_view name, meios::log_sink &log)
{
    const std::filesystem::path document =
        std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / "native_expr" / name;
    meios::source_stack sources;
    const std::vector<std::filesystem::path> roots;
    meios::eval_scope scope;
    scope.install_text_loader(meios::detail::make_yaml_text_loader(sources, roots, log));
    scope.install_yaml_parser(fake::yaml_parser_handle());
    const meios::expected<meios::expansion, meios::expansion_error> out =
        meios::expand(document_text(document), scope, sources, document,
                      meios::expansion_limits{}, log);
    return out ? out->document : std::string();
}

}

TEST_CASE("a dotted member reads a loaded mapping and chains with a subscript in either order",
          "[native][member]")
{
    CHECK(evaluate("xacro.load_yaml(limits_file).arm.shoulder.lower").rendered == "-1.5");
    CHECK(evaluate("config.arm.shoulder.upper").rendered == "1.5");
    CHECK(evaluate("config.arm['shoulder'].lower").rendered == "-1.5");
    CHECK(evaluate("config['arm'].shoulder['upper']").rendered == "1.5");
    CHECK(evaluate("config.bounds[1]").rendered == "2");
}

TEST_CASE("a member path belongs to a loaded value and never to an authored one",
          "[native][member]")
{
    const outcome refused = evaluate("authored.base");

    CHECK(refused.failed);
    CHECK(refused.kind == meios::eval_failure_kind::unsupported);
    REQUIRE_FALSE(refused.messages.empty());
    CHECK(refused.messages.front().find("not an auxiliary document") != std::string::npos);
    CHECK(evaluate("authored['base']").rendered == "1");
}

TEST_CASE("a member of a loaded sequence refuses naming the kind, and one of a loaded string "
          "reaches the text operations",
          "[native][member]")
{
    const outcome sequence = evaluate("config.bounds.first");
    const outcome scalar = evaluate("config.label.size");

    REQUIRE_FALSE(sequence.messages.empty());
    REQUIRE_FALSE(scalar.messages.empty());
    CHECK(sequence.messages.front().find("a sequence has no member") != std::string::npos);
    CHECK(scalar.messages.front().find("a string answers no member 'size'") != std::string::npos);
}

TEST_CASE("a missing member names what was asked for and none of the mapping's other keys",
          "[native][member]")
{
    const outcome absent = evaluate("config.shoudler");

    CHECK(absent.failed);
    REQUIRE(absent.messages.size() == 1);
    REQUIRE(absent.codes.size() == 1);
    CHECK(absent.codes.front() == meios::diagnostic_code::undefined_property);
    CHECK(absent.messages.front().find("shoudler") != std::string::npos);
    CHECK(absent.messages.front().find("bounds") == std::string::npos);
    CHECK(absent.messages.front().find("label") == std::string::npos);
}

TEST_CASE("a loaded mapping crosses a property, a macro argument and a nested scope and a member "
          "still reads",
          "[native][member]")
{
    meios::log_sink log;
    const std::string out = expanded("members.urdf.xacro", log);

    REQUIRE_FALSE(out.empty());
    CHECK(out.find("filename=\"package://ur_description/meshes/base.dae\"") != std::string::npos);
    CHECK(out.find("<link name=\"visual\"") != std::string::npos);
    CHECK(out.find("type=\"continuous\"") != std::string::npos);
}

// The python evaluator binds the mathematics names and its builtins bare and no namespace beside
// xacro, so only a head it binds is a reason to send the author there.
TEST_CASE("a call target's refusal advises the python evaluator only for a head that evaluator binds", "[native][member]")
{
    const outcome math = evaluate("math.pi");
    REQUIRE(math.failed);
    CHECK(math.messages.front().find("eval-python") == std::string::npos);
    CHECK(math.messages.front().find("sin(x)") != std::string::npos);
    const outcome python = evaluate("python.len");
    REQUIRE(python.failed);
    CHECK(python.messages.front().find("eval-python") == std::string::npos);
    CHECK(python.messages.front().find("len(x)") != std::string::npos);
    const outcome unbound = evaluate("nowhere.x");
    REQUIRE(unbound.failed);
    CHECK(unbound.messages.front().find("eval-python") == std::string::npos);
    const outcome builtin = evaluate("str.upper");
    REQUIRE(builtin.failed);
    CHECK(builtin.messages.front().find("eval-python") != std::string::npos);
}
