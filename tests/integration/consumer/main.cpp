#include <meios/urdf.h>
#include <meios/io.h>
#include <meios/xacro.h>
#include <meios/model.h>

#include <iostream>
#include <string_view>

namespace
{

// A model_sink defined entirely outside the meios source tree. It is deliberately
// none of the three sinks the reader ever explicitly instantiated, so it can only
// link if the parse boundary is genuinely type-erased in the installed package.
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

}

int main()
{
    meios::source_stack sources{};
    meios::core_evaluator eval;
    meios::log_sink log;
    meios::parse_context ctx{ sources, eval, log, meios::missing_asset::warn,
                              meios::topology_policy::fail, meios::material_policy::warn,
                              meios::strictness::strict, {} };
    meios::basic_parser<meios::urdf_reader> parser(ctx);
    record_counter sink;
    parser.parse(probe_urdf, sink);

    const bool ok = sink.finished && sink.robots == 1 && sink.materials == 1
                    && sink.links == 2 && sink.joints == 1;
    if(!ok)
    {
        std::cerr << "out-of-tree sink observed unexpected record counts:"
                  << " robots=" << sink.robots << " materials=" << sink.materials
                  << " links=" << sink.links << " joints=" << sink.joints
                  << " finished=" << sink.finished << '\n';
        return 1;
    }
    std::cout << "out-of-tree sink linked and ran against the installed package"
              << " (links=" << sink.links << ", joints=" << sink.joints << ")\n";
    return 0;
}
