#ifndef HPP_GUARD_MEIOS_TESTS_DIFFERENTIAL_SEED_H
#define HPP_GUARD_MEIOS_TESTS_DIFFERENTIAL_SEED_H

#include "../unit/fake_yaml_parser.h"

#include <meios/xacro.h>

#ifdef MEIOS_TEST_HAS_YAML
    #include <meios/yaml/parser.h>
#endif

#include <array>
#include <memory>
#include <string>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>

namespace differential
{

// The seed scope the pinned oracle measurement was taken under (record_upstream.py's
// seed_body()), reproduced here so meios's own evaluator reads the same names. Values only --
// ordering is immaterial for a scope's bindings.
inline constexpr std::string_view seed_yaml =
    "mesh_files:\n  base:\n    visual:\n      mesh:\n        package: ur_description\n"
    "        path: meshes/base.dae\njoint_limits:\n  shoulder_pan:\n    min: -6.28\n";

inline constexpr std::string_view sequence_yaml = "bounds:\n  - -6.28\n  - 0.0\n  - 6.28\n";

inline constexpr std::string_view collision_yaml = "keys: 5\nvalues: 6\nplain: 7\n";

inline constexpr std::string_view seeded_joints[] = { "shoulder_pan", "shoulder_lift",
                                                       "elbow_joint",  "wrist_1",
                                                       "wrist_2",      "wrist_3" };

inline constexpr std::pair<std::string_view, std::string_view> seeded_names[] = {
    { "safety_pos_margin", "0.15" }, { "mass", "3.7" },  { "radius", "0.06" },
    { "length", "0.12" },            { "name", "base" }, { "type", "visual" },
    { "wrist_3_joint_type", "continuous" }
};

class seed_loader final : public meios::text_resource_loader::fetcher
{
public:
    std::optional<std::string> fetch(std::string_view spec, const std::filesystem::path &) override
    {
        if(spec == "sequence.yaml")
            return std::string(sequence_yaml);
        return std::string(spec == "collisions.yaml" ? collision_yaml : seed_yaml);
    }
};

inline meios::eval_scope seeded_scope(meios::log_sink &log)
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
    scope.set("sequence_file", meios::value{ std::string("sequence.yaml") });
    scope.set("collision_file", meios::value{ std::string("collisions.yaml") });
    meios::core_evaluator evaluator;
    scope.set("sec_mesh_files",
              evaluator.eval("xacro.load_yaml(seed_file)['mesh_files']", scope, log));
    scope.set("sec_bounds", evaluator.eval("xacro.load_yaml(sequence_file)['bounds']", scope, log));
    scope.set("sec_collisions", evaluator.eval("xacro.load_yaml(collision_file)", scope, log));
    return scope;
}

// A key spells itself the way Python spells the key object: quoted when it is text, and by the
// shared scalar spelling otherwise, so a numeric or null key is not dressed up as a string.
inline std::string key_repr(const meios::detail::scalar_key &key)
{
    const std::optional<std::string> text = key.text();
    return text ? '\'' + *text + '\'' : meios::render_scalar(key);
}

// Python's own spelling of a value inside a collection -- built only so a fresh render can be
// compared verbatim.
inline std::string repr(const meios::value &one)
{
    if(one.kind() == meios::value_kind::string)
        return '\'' + *one.text() + '\'';
    if(one.kind() == meios::value_kind::sequence)
    {
        std::string out = "[";
        for(std::size_t at = 0; at < one.size(); ++at)
            out += (at == 0 ? "" : ", ") + repr(*one.at(at));
        return out + ']';
    }
    if(one.kind() != meios::value_kind::mapping)
        return meios::render_scalar(one).value_or("<no scalar spelling>");
    std::string out = "{";
    for(std::size_t at = 0; at < one.size(); ++at)
        out += (at == 0 ? "" : ", ") + key_repr(*one.key_at(at)) + ": " + repr(*one.at(at));
    return out + '}';
}

// A substitution carries a value into a document by Python's str(), which hands back a string's
// own text; only a string nested inside a collection is spelled with its quotes.
inline std::string rendered_as(const meios::value &one)
{
    const std::optional<std::string> text = one.text();
    return text ? *text : repr(one);
}

struct outcome
{
    bool failed;
    std::string rendered;
};

// The seeded expression-case scope -- every expressions.cases id is evaluated under it.
inline outcome evaluate_seeded(std::string_view expression)
{
    meios::log_sink silent;
    const meios::eval_scope scope = seeded_scope(silent);
    meios::core_evaluator evaluator;
    const meios::value result = evaluator.eval(expression, scope, silent);
    return { evaluator.failed(), evaluator.failed() ? std::string() : rendered_as(result) };
}

// No seeding: a divergence-manifest probe is a self-contained expression.
inline outcome evaluate_bare(std::string_view expression)
{
    meios::eval_scope scope;
    meios::log_sink silent;
    meios::core_evaluator evaluator;
    const meios::value result = evaluator.eval(expression, scope, silent);
    return { evaluator.failed(), evaluator.failed() ? std::string() : rendered_as(result) };
}

#ifdef MEIOS_TEST_HAS_YAML

// The document differential.py writes beside its probe, served here by name so both sides read
// the same bytes; upstream loads it as a value that contains itself.
inline constexpr std::string_view recursive_yaml = "a: &a [1, *a]\n";

class named_loader final : public meios::text_resource_loader::fetcher
{
public:
    std::optional<std::string> fetch(std::string_view, const std::filesystem::path &) override
    {
        return std::string(recursive_yaml);
    }
};

// A probe reading a document needs the module's own reader rather than the test double: the
// divergence it records belongs to the document reader, not to the expression grammar.
inline outcome evaluate_read(std::string_view expression)
{
    meios::eval_scope scope;
    scope.install_text_loader(meios::text_resource_loader{ std::make_unique<named_loader>() });
    scope.install_yaml_parser(meios::make_yaml_parser());
    meios::log_sink silent;
    meios::core_evaluator evaluator;
    const meios::value result = evaluator.eval(expression, scope, silent);
    return { evaluator.failed(), evaluator.failed() ? std::string() : rendered_as(result) };
}

// Spelled exactly as differential.py's own table spells them, so an id names the same rendered
// file whichever side wrote it.
struct probe
{
    bool reads;
    std::string_view id;
    std::string_view expression;
};

inline constexpr std::array<probe, 9> divergence_probes{
    probe{ false, "div_or_operand", "1 or 2" }, probe{ false, "div_and_operand", "2 and 3" },
    probe{ true, "div_self_reference", "xacro.load_yaml('recursive.yaml')['a']" },
    probe{ false, "div_dict_pair_sequence", "dict([('a', 1)])" },
    probe{ false, "div_dict_mixed_arguments", "dict([('a', 1)], b=2)" },
    probe{ false, "div_string_repetition", "'ab' * 3" },
    probe{ false, "div_string_ordering", "'a' < 'b'" },
    probe{ false, "div_string_split_whitespace", "'a b'.split()" },
    probe{ false, "div_string_split_limit", "'a b c'.split(' ', 1)" }
};

inline std::string observed_value(const probe &one)
{
    const outcome ran = one.reads ? evaluate_read(one.expression) : evaluate_bare(one.expression);
    return ran.failed ? std::string("REFUSED") : ran.rendered;
}

inline std::string trimmed(std::string text)
{
    while(!text.empty() && (text.back() == '\n' || text.back() == '\r'))
        text.pop_back();
    return text;
}

#endif

}

#endif
