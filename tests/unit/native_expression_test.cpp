#include "oracle_records.h"
#include "fake_yaml_parser.h"

#include "meios/xacro/lexer.h"
#include "meios/xacro/eval_session.h"
#include "meios/xacro/substitution_detail.h"

#include "meios/urdf/yaml_resource.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <utility>
#include <iterator>
#include <optional>
#include <filesystem>
#include <functional>
#include <string_view>

#ifndef MEIOS_URDF_FIXTURE_DIR
    #error "MEIOS_URDF_FIXTURE_DIR must name the urdf fixture root"
#endif

namespace
{

// The seed scope the upstream measurement was taken under, reproduced here. The values are
// the probe's inputs; every expectation still comes from the record itself.
constexpr std::string_view seed_document =
    "mesh_files:\n  base:\n    visual:\n      mesh:\n        package: ur_description\n"
    "        path: meshes/base.dae\njoint_limits:\n  shoulder_pan:\n    min: -6.28\n";

constexpr std::string_view seeded_joints[] = { "shoulder_pan", "shoulder_lift", "elbow_joint",
                                              "wrist_1",      "wrist_2",       "wrist_3" };

constexpr std::pair<std::string_view, std::string_view> seeded_names[] = {
    { "safety_pos_margin", "0.15" }, { "mass", "3.7" },   { "radius", "0.06" },
    { "length", "0.12" },            { "name", "base" },  { "type", "visual" },
    { "wrist_3_joint_type", "continuous" }
};

class seed_loader final : public meios::text_resource_loader::fetcher
{
public:
    std::optional<std::string> fetch(std::string_view, const std::filesystem::path &) override
    {
        return std::string(seed_document);
    }
};

// Python's own spelling of a value, built only so a recorded rendering can be compared
// verbatim. It lives in the test because the engine deliberately never writes a collection's
// contents into a document or a diagnostic.
std::string repr(const meios::value &one)
{
    if(one.kind() == meios::value_kind::string)
        return '\'' + *one.text() + '\'';
    if(one.kind() != meios::value_kind::mapping)
        return meios::render_scalar(one).value_or("<no scalar spelling>");
    std::string out = "{";
    for(std::size_t at = 0; at < one.size(); ++at)
        out += (at == 0 ? "" : ", ") + ('\'' + *one.key_at(at) + "': ") + repr(*one.at(at));
    return out + '}';
}

meios::eval_scope seeded_scope(meios::log_sink &log)
{
    meios::eval_scope scope;
    scope.install_text_loader(meios::text_resource_loader{ std::make_unique<seed_loader>() });
    scope.install_yaml_parser(fake::yaml_parser_handle());
    for(const std::pair<std::string_view, std::string_view> &one : seeded_names)
        scope.set(one.first, meios::detail::classify(one.second));
    for(std::string_view file : { "seed", "joint_limits_parameters", "kinematics_parameters",
                                 "physical_parameters", "visual_parameters" })
        scope.set(std::string(file) + "_file", meios::value{ std::string("seed.yaml") });
    std::size_t index = 0;
    for(std::string_view joint : seeded_joints)
    {
        const double magnitude = 6.0 + static_cast<double>(index++);
        scope.set(std::string(joint) + "_lower_limit", *meios::value::make_real(-magnitude));
        scope.set(std::string(joint) + "_upper_limit", *meios::value::make_real(magnitude));
    }
    meios::core_evaluator evaluator;
    scope.set("sec_mesh_files",
              evaluator.eval("xacro.load_yaml(seed_file)['mesh_files']", scope, log));
    return scope;
}

struct outcome
{
    bool failed;
    meios::eval_failure_kind kind;
    std::string rendered;
    std::vector<std::string> messages;
};

struct recorder
{
    std::vector<std::string> messages;

    void operator()(meios::level lvl, const std::string &message)
    {
        if(lvl == meios::level::error)
            messages.push_back(message);
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        (*this)(lvl, message);
    }

    void operator()(meios::level lvl, meios::diagnostic_code, const meios::source_location &,
                    const std::string &message)
    {
        (*this)(lvl, message);
    }
};

outcome evaluate(std::string_view expression)
{
    recorder heard;
    meios::log_sink_f sink{ std::ref(heard) };
    const meios::eval_scope scope = seeded_scope(sink);
    meios::core_evaluator evaluator;
    const meios::value result = evaluator.eval(expression, scope, sink);
    return outcome{ evaluator.failed(), evaluator.failure_kind(),
                    evaluator.failed() ? std::string() : repr(result), heard.messages };
}

std::vector<meios::detail::token_kind> kinds_of(std::string_view source)
{
    std::vector<meios::detail::token_kind> kinds;
    for(const meios::detail::token &one : meios::detail::tokenize(source))
        kinds.push_back(one.kind);
    return kinds;
}

std::string document_text(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

// The fixture is expanded rather than loaded, because the shape under test is the composed
// attribute text itself, before any reader gives it a meaning.
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

bool substitution_survives(meios::eval_policy policy, std::size_t token_ceiling)
{
    const meios::evaluator_limits ceilings{ 0, 0, 0, token_ceiling, 0 };
    meios::detail::eval_session session(ceilings);
    meios::eval_scope scope;
    meios::source_stack sources;
    meios::log_sink silent;
    return meios::detail::substitute_refined("value ${1 + 1}", scope, sources, "inline.xacro",
                                             policy, {}, session, silent, {})
        .has_value();
}

}

TEST_CASE("a single-quoted literal scans as one string token carrying its text",
          "[native][expression]")
{
    const std::vector<meios::detail::token> tokens = meios::detail::tokenize("'continuous'");

    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].kind == meios::detail::token_kind::string);
    REQUIRE(tokens[0].text == "'continuous'");
    REQUIRE(tokens[0].leaf.text() == std::optional<std::string>("continuous"));
    REQUIRE(tokens[1].kind == meios::detail::token_kind::end);
}

TEST_CASE("the two unmeasured string spellings scan as refusals rather than as literals",
          "[native][expression]")
{
    REQUIRE(kinds_of("'text").front() == meios::detail::token_kind::error);
    REQUIRE(kinds_of("\"text\"").front() == meios::detail::token_kind::unsupported);
    REQUIRE(kinds_of("'a\\nb'").front() == meios::detail::token_kind::unsupported);
}

TEST_CASE("subscript, dotted access and membership each scan as their own kind",
          "[native][expression]")
{
    using kind = meios::detail::token_kind;

    REQUIRE(kinds_of("a[b]")
            == std::vector<kind>{ kind::name, kind::lbracket, kind::name, kind::rbracket,
                                  kind::end });
    REQUIRE(kinds_of("xacro.load_yaml")
            == std::vector<kind>{ kind::name, kind::dot, kind::name, kind::end });
    REQUIRE(kinds_of("pin in internal")
            == std::vector<kind>{ kind::name, kind::kw_in, kind::name, kind::end });
}

TEST_CASE("a decimal point inside a number is still part of the number", "[native][expression]")
{
    const std::vector<meios::detail::token> tokens = meios::detail::tokenize("1.5");

    REQUIRE(tokens.size() == 2);
    REQUIRE(tokens[0].kind == meios::detail::token_kind::number);
    REQUIRE(tokens[0].leaf.real() == std::optional<double>(1.5));
    REQUIRE(kinds_of(".5").front() == meios::detail::token_kind::number);
}

TEST_CASE("every measured expression evaluates to the value upstream produced",
          "[native][expression]")
{
    const std::vector<oracle::row> rows = oracle::load_rows("expressions.cases");
    REQUIRE_FALSE(rows.empty());

    std::size_t classified = 0;
    for(const oracle::row &one : rows)
    {
        REQUIRE(one.fields.size() >= 3);
        INFO("case " << one.fields[0] << ": " << one.fields[1]);
        const outcome ran = evaluate(one.fields[1]);
        if(one.fields[2] == "REFUSED")
        {
            CHECK(ran.failed);
            CHECK_FALSE(ran.messages.empty());
        }
        else
        {
            CHECK_FALSE(ran.failed);
            CHECK(ran.rendered == one.fields[2]);
        }
        ++classified;
    }
    REQUIRE(classified == rows.size());
}

TEST_CASE("a subscript chain reads by a literal key, by a variable key and through two levels",
          "[native][expression]")
{
    CHECK(evaluate("sec_mesh_files['base']['visual']['mesh']['package']").rendered
          == "'ur_description'");
    CHECK(evaluate("sec_mesh_files[name][type]['mesh']['path']").rendered
          == "'meshes/base.dae'");
    CHECK(evaluate("xacro.load_yaml(seed_file)['joint_limits']['shoulder_pan']['min']").rendered
          == "-6.28");
}

TEST_CASE("a subscript names what went wrong without naming what the mapping holds",
          "[native][expression]")
{
    const outcome absent = evaluate("sec_mesh_files['missing']");
    CHECK(absent.failed);
    REQUIRE(absent.messages.size() == 1);
    CHECK(absent.messages.front().find("missing") != std::string::npos);
    CHECK(absent.messages.front().find("base") == std::string::npos);

    CHECK(evaluate("mass['base']").messages.front().find("subscripted") != std::string::npos);
    CHECK(evaluate("sec_mesh_files[3]").messages.front().find("subscript key")
          != std::string::npos);
}

TEST_CASE("a string compares only against another string and never becomes a number",
          "[native][expression]")
{
    CHECK(evaluate("wrist_3_joint_type == 'continuous'").rendered == "True");
    CHECK(evaluate("'a' != 'b'").rendered == "True");
    CHECK(evaluate("wrist_3_joint_type != 3").rendered == "True");
    CHECK(evaluate("wrist_3_joint_type == 3").rendered == "False");
    CHECK(evaluate("'2' == 2").rendered == "False");

    const outcome ordered = evaluate("'a' < 'b'");
    CHECK(ordered.failed);
    CHECK(ordered.messages.front().find("ordering comparison") != std::string::npos);
}

// A concatenation is real Python the CPython backend can run, so it stays in the one category
// a lenient policy may soften; a missing key or an unsubscriptable value stays terminal.
TEST_CASE("a string reaching arithmetic is refused as unsupported, not as a fault",
          "[native][expression]")
{
    CHECK(evaluate("wrist_3_joint_type + 'x'").kind == meios::eval_failure_kind::unsupported);
    CHECK(evaluate("wrist_3_joint_type * 2").kind == meios::eval_failure_kind::unsupported);
    CHECK(evaluate("sec_mesh_files['missing']").kind == meios::eval_failure_kind::error);
    CHECK(evaluate("sec_mesh_files + 1").kind == meios::eval_failure_kind::error);
}

TEST_CASE("membership tests a mapping's keys and refuses every other right-hand kind",
          "[native][expression]")
{
    CHECK(evaluate("type in sec_mesh_files[name]").rendered == "True");
    CHECK(evaluate("'absent' in sec_mesh_files").rendered == "False");

    const outcome scalar = evaluate("name in mass");
    CHECK(scalar.failed);
    CHECK(scalar.messages.front().find("membership") != std::string::npos);

    // Either order of a mixed run puts a boolean beside the membership test, and a boolean
    // is never a mapping key, so neither can quietly mean what Python's single comparison
    // level would have made of it.
    CHECK(evaluate("type in sec_mesh_files[name] == True").failed);
    CHECK(evaluate("type == 'visual' in sec_mesh_files").failed);
}

TEST_CASE("the namespaced document call reaches bytes only through the scope",
          "[native][expression]")
{
    CHECK(evaluate("xacro.load_yaml(seed_file)['mesh_files'][name][type]['mesh']['package']")
              .rendered
          == "'ur_description'");

    meios::eval_scope bare;
    bare.set("seed_file", meios::value{ std::string("seed.yaml") });
    recorder heard;
    meios::log_sink_f sink{ std::ref(heard) };
    meios::core_evaluator evaluator;
    evaluator.eval("xacro.load_yaml(seed_file)", bare, sink);

    CHECK(evaluator.failed());
    REQUIRE_FALSE(heard.messages.empty());
    CHECK(heard.messages.front().find("auxiliary document") != std::string::npos);
}

TEST_CASE("a construct outside the measured surface refuses by name", "[native][expression]")
{
    const std::pair<std::string_view, std::string_view> refused[] = {
        { "sec_mesh_files.base", "sec_mesh_files.base" },
        { "sec_mesh_files[name].mesh", "dotted access on a value" },
        { "dict(a=1)", "dict()" },
        { "sec_mesh_files[1:2]", "unsupported subscript form" },
        { "[x for x in sec_mesh_files]", "unsupported expression at '['" },
        { "\"continuous\"", "unsupported expression at '\"'" },
    };

    for(const std::pair<std::string_view, std::string_view> &one : refused)
    {
        INFO("expression: " << one.first);
        const outcome ran = evaluate(one.first);
        CHECK(ran.failed);
        REQUIRE_FALSE(ran.messages.empty());
        CHECK(ran.messages.front().find(one.second) != std::string::npos);
    }
}

TEST_CASE("a mapping crosses a property, a macro argument and a nested scope and still reads",
          "[native][expression]")
{
    meios::log_sink log;
    const std::string out = expanded("subscripts.urdf.xacro", log);

    REQUIRE_FALSE(out.empty());
    CHECK(out.find("filename=\"package://ur_description/meshes/base.dae\"") != std::string::npos);
    CHECK(out.find("<link name=\"visual\"") != std::string::npos);
}

TEST_CASE("a boolean read out of an auxiliary document drives a conditional",
          "[native][expression]")
{
    meios::log_sink log;
    const std::string out = expanded("subscripts.urdf.xacro", log);

    REQUIRE_FALSE(out.empty());
    CHECK(out.find("type=\"continuous\"") != std::string::npos);
    CHECK(out.find("type=\"revolute\"") == std::string::npos);
}

TEST_CASE("an expression over the token ceiling halts under every evaluation policy",
          "[native][expression]")
{
    REQUIRE(substitution_survives(meios::eval_policy::fail, 64));
    REQUIRE(substitution_survives(meios::eval_policy::warn, 64));
    REQUIRE(substitution_survives(meios::eval_policy::skip, 64));

    REQUIRE_FALSE(substitution_survives(meios::eval_policy::fail, 1));
    REQUIRE_FALSE(substitution_survives(meios::eval_policy::warn, 1));
    REQUIRE_FALSE(substitution_survives(meios::eval_policy::skip, 1));
}
