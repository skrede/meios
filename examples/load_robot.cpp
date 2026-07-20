#include "example_paths.h"

#include <meios/urdf.h>
#include <meios/model.h>

#include <string>
#include <cstddef>
#include <iostream>
#include <unordered_map>

int main()
{
    const meios::expected<meios::model<double>, meios::load_error> robot =
        meios::load(examples::robot_description);
    if(!robot)
    {
        std::cout << "load failed: " << robot.error().message << '\n';
        return 1;
    }

    const meios::model<double> &model = *robot;

    std::cout << "loaded " << model.links.size() << " links\n";

    std::cout << "root-first order:";
    for(const int index : model.topo.order)
        std::cout << ' ' << model.links[static_cast<std::size_t>(index)].name;
    std::cout << '\n';

    const std::unordered_map<std::string, int>::const_iterator found =
        model.joint_index.find("base_to_upper");
    if(found != model.joint_index.end())
    {
        const meios::joint<double> &edge = model.joints[static_cast<std::size_t>(found->second)];
        std::cout << "base_to_upper origin z = " << edge.origin.translation.z
                  << " (the substituted upper_length property)\n";
    }

    return 0;
}
