#include "fixture_path.h"

#include <meios/io.h>
#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/xacro.h>

#include <string>
#include <vector>
#include <cstddef>
#include <iostream>
#include <algorithm>
#include <string_view>
#include <unordered_map>

namespace
{

// A model_sink defined entirely outside the meios source tree. It is deliberately
// none of the sinks the reader ever explicitly instantiated, so it can only link if
// the parse boundary is genuinely type-erased in the installed package.
struct record_counter
{
    int robots = 0;
    int materials = 0;
    int links = 0;
    int joints = 0;
    bool finished = false;

    void on_robot(const meios::robot_info &) { ++robots; }
    void on_material(const meios::material<double> &) { ++materials; }
    void on_link(const meios::link<double> &) { ++links; }
    void on_joint(const meios::joint<double> &) { ++joints; }
    void finish() { finished = true; }
};

constexpr std::string_view probe_urdf = R"(<?xml version="1.0"?>
<robot name="counter_probe">
  <material name="grey">
    <color rgba="0.5 0.5 0.5 1.0"/>
  </material>
  <link name="base_link"/>
  <link name="tool"/>
  <joint name="base_to_tool" type="fixed">
    <parent link="base_link"/>
    <child link="tool"/>
    <origin xyz="0 0 0.1" rpy="0 0 0"/>
  </joint>
</robot>
)";

struct diagnostic_record
{
    meios::level lvl;
    meios::diagnostic_code code;
    std::string message;
};

// A diagnostic sink living entirely in the consumer's tree: it lifts every library
// diagnostic straight into the consumer's own diagnostic_record. The four-argument
// overload is the one that carries the typed diagnostic_code across the install
// boundary, so a code-bearing diagnostic lands with its code intact.
class diagnostic_lift
{
public:
    explicit diagnostic_lift(std::vector<diagnostic_record> &out) : m_out(out) {}

    void operator()(meios::level lvl, const std::string &message)
    {
        m_out.push_back({ lvl, meios::diagnostic_code::unspecified, message });
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        m_out.push_back({ lvl, meios::diagnostic_code::unspecified, message });
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &,
                    const std::string &message)
    {
        m_out.push_back({ lvl, code, message });
    }

private:
    std::vector<diagnostic_record> &m_out;
};

bool nearly_equal(double a, double b) { return (a < b ? b - a : a - b) < 1e-9; }

int run_out_of_tree_sink()
{
    meios::source_stack sources{};
    meios::core_evaluator eval;
    meios::log_sink log;
    meios::parse_context ctx{ sources, eval, log, meios::missing_asset::warn,
                              meios::topology_policy::fail, meios::material_policy::warn,
                              meios::strictness::fail, {} };
    meios::basic_parser<meios::urdf_reader> parser(ctx);
    record_counter sink;
    parser.parse(probe_urdf, sink);

    const bool ok = sink.finished && sink.robots == 1 && sink.materials == 1
                    && sink.links == 2 && sink.joints == 1;
    if(!ok)
    {
        std::cerr << "out-of-tree model_sink observed unexpected record counts:"
                  << " robots=" << sink.robots << " materials=" << sink.materials
                  << " links=" << sink.links << " joints=" << sink.joints
                  << " finished=" << sink.finished << '\n';
        return 1;
    }
    std::cout << "out-of-tree model_sink linked and ran against the installed package"
              << " (links=" << sink.links << ", joints=" << sink.joints << ")\n";
    return 0;
}

int run_vendored_load()
{
    std::vector<diagnostic_record> diagnostics;
    meios::log_sink_f diagnostic_sink{ diagnostic_lift{ diagnostics } };

    meios::load_options opts;
    opts.materials = meios::material_policy::warn;

    const meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load(consumer::vendored_robot, opts, diagnostic_sink);
    if(!loaded)
    {
        std::cerr << "load() of the vendored xacro failed: ("
                  << meios::to_string(loaded.error().code) << ") "
                  << loaded.error().message << '\n';
        return 1;
    }
    const meios::model<double> &model = loaded->robot;

    const auto by_name = [&](const std::unordered_map<std::string, int> &index,
                             const std::string &name) -> int {
        const std::unordered_map<std::string, int>::const_iterator it = index.find(name);
        return it == index.end() ? -1 : it->second;
    };

    const int base = by_name(model.link_index, "base_link");
    const int shoulder = by_name(model.link_index, "shoulder_link");
    const int joint = by_name(model.joint_index, "base_to_shoulder");

    if(model.links.size() != 3)
    {
        std::cerr << "vendored xacro yielded " << model.links.size() << " links, expected 3\n";
        return 1;
    }
    if(model.topo.order.empty() || model.topo.order.front() != base)
    {
        std::cerr << "topology order is not root-first\n";
        return 1;
    }
    if(base < 0 || shoulder < 0
       || model.topo.parent_of[static_cast<std::size_t>(shoulder)] != base)
    {
        std::cerr << "shoulder_link is not parented on base_link\n";
        return 1;
    }
    if(joint < 0
       || !nearly_equal(model.joints[static_cast<std::size_t>(joint)].origin.translation.z, 0.25))
    {
        std::cerr << "substituted origin xyz did not survive expansion\n";
        return 1;
    }

    const bool carried_code = std::any_of(diagnostics.begin(), diagnostics.end(),
        [](const diagnostic_record &record) {
            return record.code == meios::diagnostic_code::undefined_material
                   && record.lvl == meios::level::warn;
        });
    if(!carried_code)
    {
        std::cerr << "the consumer's diagnostic sink never received a typed diagnostic_code\n";
        return 1;
    }

    std::cout << "loaded the vendored xacro through the installed load() surface"
              << " (links=" << model.links.size()
              << ", substituted origin z=0.25, diagnostics=" << diagnostics.size() << ")\n";
    return 0;
}

}

int main()
{
    if(const int rc = run_out_of_tree_sink())
        return rc;
    if(const int rc = run_vendored_load())
        return rc;
    return 0;
}
