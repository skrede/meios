#include "verbs.h"
#include "json_escape.h"
#include "deferred_error_sink.h"

#include "meios/urdf/load.h"

#include "meios/model/model.h"
#include "meios/records/link.h"
#include "meios/records/joint.h"
#include "meios/records/visual.h"
#include "meios/records/geometry.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <map>
#include <string>
#include <cstddef>
#include <ostream>
#include <variant>
#include <iostream>
#include <string_view>

namespace meios::cli
{

namespace
{

std::string_view joint_kind_name(joint_kind kind)
{
    switch(kind)
    {
        case joint_kind::fixed:      return "fixed";
        case joint_kind::revolute:   return "revolute";
        case joint_kind::continuous: return "continuous";
        case joint_kind::prismatic:  return "prismatic";
        case joint_kind::floating:   return "floating";
        case joint_kind::planar:     return "planar";
    }
    return "unknown";
}

std::size_t degrees_of_freedom(const model<double> &robot)
{
    std::size_t dof = 0;
    for(const joint<double> &edge : robot.joints)
        if(edge.kind != joint_kind::fixed)
            ++dof;
    return dof;
}

void print_joint_histogram(const model<double> &robot, std::ostream &out)
{
    std::map<std::string_view, std::size_t> histogram;
    for(const joint<double> &edge : robot.joints)
        ++histogram[joint_kind_name(edge.kind)];
    for(const std::pair<const std::string_view, std::size_t> &bin : histogram)
        out << "joint." << bin.first << ": " << bin.second << '\n';
}

void print_geometry_mesh(const geometry<double> &geom, std::ostream &out)
{
    const mesh<double> *shape = std::get_if<mesh<double>>(&geom.shape);
    if(shape == nullptr)
        return;
    out << "mesh: " << shape->filename;
    if(shape->resolved_path)
        out << " -> " << *shape->resolved_path;
    out << '\n';
}

void print_mesh_list(const model<double> &robot, std::ostream &out)
{
    for(const link<double> &node : robot.links)
    {
        for(const visual<double> &item : node.visuals)
            print_geometry_mesh(item.geom, out);
        for(const collision<double> &item : node.collisions)
            print_geometry_mesh(item.geom, out);
    }
}

void print_info(const model<double> &robot, std::ostream &out)
{
    out << "name: " << robot.name << '\n';
    out << "links: " << robot.links.size() << '\n';
    out << "joints: " << robot.joints.size() << '\n';
    out << "materials: " << robot.materials.size() << '\n';
    out << "dof: " << degrees_of_freedom(robot) << '\n';
    print_joint_histogram(robot, out);
    print_mesh_list(robot, out);
}

void write_histogram_json(const model<double> &robot, std::ostream &out)
{
    std::map<std::string_view, std::size_t> histogram;
    for(const joint<double> &edge : robot.joints)
        ++histogram[joint_kind_name(edge.kind)];
    out << '{';
    bool first = true;
    for(const std::pair<const std::string_view, std::size_t> &bin : histogram)
    {
        out << (first ? "" : ",") << '"' << bin.first << "\":" << bin.second;
        first = false;
    }
    out << '}';
}

void write_mesh_json(const geometry<double> &geom, std::ostream &out, bool &first)
{
    const mesh<double> *shape = std::get_if<mesh<double>>(&geom.shape);
    if(shape == nullptr)
        return;
    out << (first ? "" : ",") << "{\"filename\":\"" << json_escape(shape->filename)
        << "\",\"resolved\":";
    if(shape->resolved_path)
        out << '"' << json_escape(*shape->resolved_path) << '"';
    else
        out << "null";
    out << '}';
    first = false;
}

void write_meshes_json(const model<double> &robot, std::ostream &out)
{
    out << '[';
    bool first = true;
    for(const link<double> &node : robot.links)
    {
        for(const visual<double> &item : node.visuals)
            write_mesh_json(item.geom, out, first);
        for(const collision<double> &item : node.collisions)
            write_mesh_json(item.geom, out, first);
    }
    out << ']';
}

void print_info_json(const model<double> &robot, std::ostream &out)
{
    out << "{\"name\":\"" << json_escape(robot.name) << '"'
        << ",\"links\":" << robot.links.size()
        << ",\"joints\":" << robot.joints.size()
        << ",\"materials\":" << robot.materials.size()
        << ",\"dof\":" << degrees_of_freedom(robot) << ",\"joints_by_kind\":";
    write_histogram_json(robot, out);
    out << ",\"meshes\":";
    write_meshes_json(robot, out);
    out << "}\n";
}

}

int run_info(const verb_context &ctx)
{
    log_sink_s log(std::cerr);
    deferred_error_sink sink(log);
    load_options opts;
    opts.package_roots = to_paths(ctx.package_paths);
    source_stack sources = build_sources(opts.package_roots, sink);
    const expected<model<double>, load_error> loaded = load(positional(ctx, 0), opts, sources, sink);
    if(!loaded)
    {
        log.log(level::error, loaded.error().code, loaded.error().loc, loaded.error().message);
        return 1;
    }
    if(sink.errors() != 0)
    {
        sink.replay_first(log);
        return 1;
    }
    const model<double> &robot = *loaded;

    const auto format = ctx.value_flags.find("--format");
    if(format == ctx.value_flags.end() || format->second.empty())
    {
        print_info(robot, std::cout);
        return 0;
    }
    if(format->second != "json")
    {
        log.log(level::error, "unknown --format value: " + format->second);
        return 2;
    }
    print_info_json(robot, std::cout);
    return 0;
}

}
