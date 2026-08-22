#include "example_paths.h"

#include <meios/urdf.h>
#include <meios/model.h>

#include <string>
#include <cstddef>
#include <iostream>
#include <unordered_map>

int main()
{
    meios::load_options options;
    options.args["link_gap"] = "0.42";
    options.eval = meios::eval_policy::fail;

    meios::log_sink_s log(std::cerr);

    const meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load(examples::parametric_description, options, log);
    if(!loaded)
    {
        std::cout << "load failed: " << loaded.error().message << '\n';
        return 1;
    }

    const meios::model<double> &model = loaded->robot;

    const std::unordered_map<std::string, int>::const_iterator found =
        model.joint_index.find("base_to_tool");
    if(found == model.joint_index.end())
    {
        std::cout << "base_to_tool joint not found\n";
        return 1;
    }

    const meios::joint<double> &edge = model.joints[static_cast<std::size_t>(found->second)];
    std::cout << "arg-driven origin z = " << edge.origin.translation.z
              << " (from the link_gap:=0.42 override, no flatten step)\n";

    return 0;
}
