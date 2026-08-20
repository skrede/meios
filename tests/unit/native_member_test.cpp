#include "oracle_records.h"
#include "fake_yaml_parser.h"

#include "meios/xacro/eval_parser.h"

#include "meios/urdf/yaml_resource.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <catch2/catch_test_macros.hpp>

#include <span>
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

constexpr std::string_view limits_document =
    "arm:\n  shoulder:\n    lower: -1.5\n    upper: 1.5\nbounds:\n  - 1\n  - 2\nlabel: text\n";

// Built from the measurement rather than written out, so the document under test carries exactly
// the names upstream was observed to answer itself, plus one name reached by the prefix rule.
std::string measured_document()
{
    std::string out = "__class__: -1\n";
    std::size_t at = 0;
    for(const oracle::row &one : oracle::load_rows("collisions.cases"))
        out += one.fields.front() + ": " + std::to_string(at++) + '\n';
    return out;
}

class two_documents final : public meios::text_resource_loader::fetcher
{
public:
    std::optional<std::string> fetch(std::string_view spec, const std::filesystem::path &) override
    {
        if(spec == "measured.yaml")
            return measured_document();
        return std::string(limits_document);
    }
};

struct recorder
{
    std::vector<std::string> messages;
    std::vector<meios::diagnostic_code> codes;

    void operator()(meios::level lvl, const std::string &message)
    {
        if(lvl == meios::level::error)
            messages.push_back(message);
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        (*this)(lvl, message);
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &,
                    const std::string &message)
    {
        if(lvl == meios::level::error)
            codes.push_back(code);
        (*this)(lvl, message);
    }
};

struct outcome
{
    bool failed;
    meios::eval_failure_kind kind;
    std::string rendered;
    std::vector<std::string> messages;
    std::vector<meios::diagnostic_code> codes;
};

meios::value authored_mapping()
{
    std::vector<meios::value::entry> entries;
    entries.emplace_back("base", meios::value{ std::int64_t{ 1 } });
    entries.emplace_back("other", meios::value{ std::int64_t{ 2 } });
    return meios::value::make_mapping(std::move(entries));
}

meios::eval_scope probe_scope(meios::log_sink &log)
{
    meios::eval_scope scope;
    scope.install_text_loader(meios::text_resource_loader{ std::make_unique<two_documents>() });
    scope.install_yaml_parser(fake::yaml_parser_handle());
    scope.set("limits_file", meios::value{ std::string("limits.yaml") });
    scope.set("measured_file", meios::value{ std::string("measured.yaml") });
    scope.set("authored", authored_mapping());
    meios::core_evaluator evaluator;
    scope.set("config", evaluator.eval("xacro.load_yaml(limits_file)", scope, log));
    scope.set("kit", evaluator.eval("xacro.load_yaml(measured_file)", scope, log));
    return scope;
}

outcome evaluate(const std::string &expression)
{
    recorder heard;
    meios::log_sink_f sink{ std::ref(heard) };
    const meios::eval_scope scope = probe_scope(sink);
    meios::core_evaluator evaluator;
    const meios::value result = evaluator.eval(expression, scope, sink);
    return outcome{ evaluator.failed(), evaluator.failure_kind(),
                    evaluator.failed() ? std::string()
                                       : meios::render_scalar(result).value_or("<collection>"),
                    heard.messages, heard.codes };
}

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

bool colliding(const oracle::row &one)
{
    return one.fields.size() == 3 && one.fields[1] != one.fields[2];
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

TEST_CASE("the refusing member set is the measured one, name for name", "[native][member]")
{
    std::vector<std::string> measured;
    for(const oracle::row &one : oracle::load_rows("collisions.cases"))
        if(colliding(one))
            measured.push_back(one.fields.front());
    const std::span<const std::string_view> carried = meios::detail::colliding_members();

    REQUIRE(carried.size() == measured.size());
    for(std::size_t at = 0; at < measured.size(); ++at)
        CHECK(carried[at] == measured[at]);
}

TEST_CASE("every measured collision refuses as a member and reads as a subscript",
          "[native][member]")
{
    std::size_t at = 0;
    for(const oracle::row &one : oracle::load_rows("collisions.cases"))
    {
        const std::string name = one.fields.front();
        const std::string held = std::to_string(at++);
        INFO("name: " << name);
        const outcome member = evaluate("kit." + name);
        if(colliding(one))
        {
            REQUIRE(member.failed);
            CHECK(member.messages.front().find("['" + name + "']") != std::string::npos);
            CHECK(member.messages.front().find("mapping operation") != std::string::npos);
        }
        else
        {
            CHECK(member.rendered == held);
        }
        CHECK(evaluate("kit['" + name + "']").rendered == held);
    }
}

TEST_CASE("a member written with a leading double underscore refuses by the prefix",
          "[native][member]")
{
    const outcome dunder = evaluate("kit.__class__");

    CHECK(dunder.failed);
    REQUIRE_FALSE(dunder.messages.empty());
    CHECK(dunder.messages.front().find("leading '__'") != std::string::npos);
    CHECK(dunder.messages.front().find("__class__") == std::string::npos);
    CHECK(evaluate("kit['__class__']").rendered == "-1");
}

TEST_CASE("a collision refusal carries none of the mapping's other keys", "[native][member]")
{
    const outcome refused = evaluate("kit.keys");

    REQUIRE(refused.messages.size() == 1);
    CHECK(refused.messages.front().find("items") == std::string::npos);
    CHECK(refused.messages.front().find("popitem") == std::string::npos);
    CHECK(refused.messages.front().find("setdefault") == std::string::npos);
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
