#ifndef HPP_GUARD_MEIOS_TESTS_DIFFERENTIAL_SEED_H
#define HPP_GUARD_MEIOS_TESTS_DIFFERENTIAL_SEED_H

#include "../unit/fake_yaml_parser.h"

#include <meios/xacro.h>

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
    std::optional<std::string> fetch(std::string_view, const std::filesystem::path &) override
    {
        return std::string(seed_yaml);
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
    meios::core_evaluator evaluator;
    scope.set("sec_mesh_files",
              evaluator.eval("xacro.load_yaml(seed_file)['mesh_files']", scope, log));
    return scope;
}

// Python's own spelling of a value, matching upstream's str()/repr() of an evaluated
// expression -- built only so a fresh render can be compared verbatim.
inline std::string repr(const meios::value &one)
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
    return { evaluator.failed(), evaluator.failed() ? std::string() : repr(result) };
}

// No seeding: the divergence-manifest probes are self-contained expressions.
inline outcome evaluate_bare(std::string_view expression)
{
    meios::eval_scope scope;
    meios::log_sink silent;
    meios::core_evaluator evaluator;
    const meios::value result = evaluator.eval(expression, scope, silent);
    return { evaluator.failed(), evaluator.failed() ? std::string() : repr(result) };
}

}

#endif
