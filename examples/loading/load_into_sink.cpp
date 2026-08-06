#include "example_paths.h"

#include <meios/urdf.h>
#include <meios/model.h>

#include <string>
#include <vector>
#include <iostream>

namespace
{

struct scene_builder
{
    void on_robot(const meios::robot_info &robot)
    {
        name = robot.name;
    }

    void on_material(const meios::material<double> &)
    {
    }

    void on_link(const meios::link<double> &node)
    {
        bodies.push_back(node.name);
    }

    void on_joint(const meios::joint<double> &edge)
    {
        articulations.push_back(edge.name);
    }

    void finish()
    {
        built = true;
    }

    bool built;
    std::string name;
    std::vector<std::string> bodies;
    std::vector<std::string> articulations;
};

static_assert(meios::model_sink<scene_builder>);

bool arrived(const scene_builder &scene)
{
    if(!scene.built)
    {
        std::cout << "the load failed and left the scene exactly as it was\n";
        return false;
    }

    if(scene.bodies.empty())
    {
        std::cout << "the load succeeded but handed the scene no bodies at all\n";
        return false;
    }

    return true;
}

void report(const scene_builder &scene, meios::completeness claims)
{
    std::cout << "scene \"" << scene.name << "\" holds " << scene.bodies.size() << " bodies and "
              << scene.articulations.size() << " articulations\n";
    for(const std::string &body : scene.bodies)
        std::cout << "  body " << body << '\n';
    for(const std::string &articulation : scene.articulations)
        std::cout << "  articulation " << articulation << '\n';

    if(has(claims, meios::completeness::deployment_complete))
        std::cout << "every mesh and texture the description named was found, so the scene can be "
                     "drawn as authored\n";
    else
        std::cout << "at least one mesh or texture was not found, so the scene is missing "
                     "geometry it asked for\n";
}

}

int main()
{
    scene_builder scene{};
    meios::log_sink_s log(std::cerr);
    const meios::load_options options;

    const meios::load_summary summary =
        meios::load_into(examples::robot_description, options, scene, log);

    if(!arrived(scene))
        return 1;

    report(scene, summary.claims);

    return 0;
}
