#include "fixture_path.h"
#include "consumer_probe.h"

#include <meios/io.h>
#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/xacro.h>

#include <string>
#include <vector>
#include <cstddef>
#include <iostream>
#include <algorithm>
#include <unordered_map>

namespace
{

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
    consumer::record_counter sink;
    parser.parse(consumer::probe_urdf, sink);

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
    std::vector<consumer::diagnostic_record> diagnostics;
    meios::log_sink_f diagnostic_sink{ consumer::diagnostic_lift{ diagnostics } };

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
        [](const consumer::diagnostic_record &record) {
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
    if(const int rc = consumer::run_facade())
        return rc;
    if(const int rc = consumer::run_memory_source())
        return rc;
    return 0;
}
