#ifndef HPP_GUARD_MEIOS_TEST_NATIVE_EXPRESSION_FIXTURE_H
#define HPP_GUARD_MEIOS_TEST_NATIVE_EXPRESSION_FIXTURE_H

#include "fake_yaml_parser.h"

#include <meios/xacro.h>

#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <optional>
#include <filesystem>
#include <functional>
#include <string_view>

namespace native_expression_test
{

// The seed scope the upstream measurement was taken under, reproduced here. The values are
// the probe's inputs; every expectation still comes from the record itself.
inline constexpr std::string_view seed_document = "mesh_files:\n  base:\n    visual:\n      mesh:\n        package: ur_description\n"
                                                  "        path: meshes/base.dae\njoint_limits:\n  shoulder_pan:\n    min: -6.28\n";

inline constexpr std::string_view sequence_document = "bounds:\n  - -6.28\n  - 0.0\n  - 6.28\n";

inline constexpr std::string_view collision_document = "keys: 5\nvalues: 6\nplain: 7\n";

inline constexpr std::string_view seeded_joints[] = {"shoulder_pan", "shoulder_lift", "elbow_joint", "wrist_1", "wrist_2", "wrist_3"};

inline constexpr std::pair<std::string_view, std::string_view> seeded_names[] = {
        {"safety_pos_margin", "0.15"}, {"mass", "3.7"}, {"radius", "0.06"}, {"length", "0.12"}, {"name", "base"}, {"type", "visual"}, {"wrist_3_joint_type", "continuous"}};

class seed_loader final : public meios::text_resource_loader::fetcher
{
public:
    meios::text_resource_result fetch(std::string_view spec, const std::filesystem::path &) override
    {
        if(spec == "sequence.yaml")
            return std::string(sequence_document);
        return std::string(spec == "collisions.yaml" ? collision_document : seed_document);
    }
};

// A key spells itself the way Python spells the key object: quoted when it is text, and by the
// shared scalar spelling otherwise, so a numeric or null key is not dressed up as a string.
inline std::string key_repr(const meios::detail::scalar_key &key)
{
    const std::optional<std::string> text = key.text();
    return text ? '\'' + *text + '\'' : meios::render_scalar(key);
}

// Python's own spelling of a value, built only so a recorded rendering can be compared
// verbatim. It lives in the test because the engine deliberately never writes a collection's
// contents into a document or a diagnostic.
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

inline meios::eval_scope seeded_scope(meios::log_sink &log)
{
    meios::eval_scope scope;
    scope.install_text_loader(meios::text_resource_loader{std::make_unique<seed_loader>()});
    scope.install_yaml_parser(fake::yaml_parser_handle());
    for(const std::pair<std::string_view, std::string_view> &one : seeded_names)
        scope.set(one.first, meios::detail::classify(one.second));
    for(std::string_view file : {"seed", "joint_limits_parameters", "kinematics_parameters", "physical_parameters", "visual_parameters"})
        scope.set(std::string(file) + "_file", meios::value{std::string("seed.yaml")});
    std::size_t index = 0;
    for(std::string_view joint : seeded_joints)
    {
        const double magnitude = 6.0 + static_cast<double>(index++);
        scope.set(std::string(joint) + "_lower_limit", *meios::value::make_real(-magnitude));
        scope.set(std::string(joint) + "_upper_limit", *meios::value::make_real(magnitude));
    }
    scope.set("sequence_file", meios::value{std::string("sequence.yaml")});
    scope.set("collision_file", meios::value{std::string("collisions.yaml")});
    meios::core_evaluator evaluator;
    scope.set("sec_mesh_files", evaluator.eval("xacro.load_yaml(seed_file)['mesh_files']", scope, log));
    scope.set("sec_bounds", evaluator.eval("xacro.load_yaml(sequence_file)['bounds']", scope, log));
    scope.set("sec_collisions", evaluator.eval("xacro.load_yaml(collision_file)", scope, log));
    return scope;
}

struct outcome
{
    bool failed;
    meios::eval_failure_kind kind;
    std::string rendered;
    std::vector<std::string> messages;
    std::vector<meios::diagnostic_code> codes;
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

    void operator()(meios::level lvl, meios::diagnostic_code found, const meios::source_location &, const std::string &message)
    {
        if(lvl == meios::level::error)
            codes.push_back(found);
        (*this)(lvl, message);
    }
};

inline outcome evaluate(std::string_view expression)
{
    recorder heard;
    meios::log_sink_f sink{std::ref(heard)};
    const meios::eval_scope scope = seeded_scope(sink);
    meios::core_evaluator evaluator;
    const meios::value result = evaluator.eval(expression, scope, sink);
    return outcome{evaluator.failed(), evaluator.failure_kind(), evaluator.failed() ? std::string() : rendered_as(result), heard.messages, heard.codes};
}

}

#endif
