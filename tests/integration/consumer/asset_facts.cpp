#include "model_facts.h"

#include <meios/model.h>

#include <map>
#include <string>
#include <vector>
#include <cstddef>
#include <variant>
#include <optional>
#include <algorithm>
#include <filesystem>

namespace consumer
{

namespace
{

int note_mesh(const meios::geometry<double> &geom, std::vector<std::string> &seen)
{
    if(!std::holds_alternative<meios::mesh<double>>(geom.shape))
        return 0;
    const meios::mesh<double> &found = std::get<meios::mesh<double>>(geom.shape);
    if(!found.resolved_path.has_value())
        return refuse_fact("mesh " + found.filename, "a path under the package root", "unresolved");
    if(!std::filesystem::exists(*found.resolved_path))
        return refuse_fact("mesh " + found.filename, "a file that exists", *found.resolved_path);
    if(std::find(seen.begin(), seen.end(), found.filename) == seen.end())
        seen.push_back(found.filename);
    return 0;
}

// Distinct references in document order, which is the order the record lists them in: within a
// link the description declares its visuals before its collisions.
int gather_meshes(const meios::model<double> &robot, std::vector<std::string> &seen)
{
    for(const meios::link<double> &one : robot.links)
    {
        for(const meios::visual<double> &part : one.visuals)
            if(const int rc = note_mesh(part.geom, seen))
                return rc;
        for(const meios::collision<double> &part : one.collisions)
            if(const int rc = note_mesh(part.geom, seen))
                return rc;
    }
    return 0;
}

int check_meshes(const std::vector<oracle::row> &rows, const meios::model<double> &robot)
{
    std::vector<std::string> seen;
    if(const int rc = gather_meshes(robot, seen))
        return rc;
    const std::size_t recorded = rows_under(rows, "mesh.filename.");
    if(seen.size() != recorded)
        return refuse_fact("mesh reference count", std::to_string(recorded),
                           std::to_string(seen.size()));
    for(std::size_t at = 0; at < seen.size(); ++at)
        if(const int rc = check_text(rows, "mesh.filename." + std::to_string(at), seen[at]))
            return rc;
    return 0;
}

int check_triple(const std::map<std::string, std::vector<double>> &declared, const std::string &key,
                 const std::string &field, const std::vector<double> &loaded)
{
    const std::map<std::string, std::vector<double>>::const_iterator found = declared.find(field);
    if(found == declared.end())
        return 0;
    if(found->second.size() != loaded.size())
        return refuse_fact(key + " " + field, std::to_string(found->second.size()) + " numbers",
                           std::to_string(loaded.size()) + " numbers");
    for(std::size_t at = 0; at < loaded.size(); ++at)
        if(!same_number(found->second[at], loaded[at]))
            return refuse_fact(key + " " + field, text_of(found->second[at]), text_of(loaded[at]));
    return 0;
}

// A part the description places by nothing carries no recorded row, so an absent key is not a
// failure here; the caller counts what was compared and holds the record to that count.
int check_origin(const std::vector<oracle::row> &rows, const std::string &key,
                 const meios::transform<double> &placed, std::size_t &compared)
{
    const std::optional<oracle::row> found = oracle::lookup(rows, key);
    if(!found || found->fields.size() < 2)
        return 0;
    ++compared;
    const std::map<std::string, std::vector<double>> declared = keyed_numbers(found->fields[1]);
    const std::vector<double> xyz{ placed.translation.x, placed.translation.y,
                                   placed.translation.z };
    if(const int rc = check_triple(declared, key, "xyz", xyz))
        return rc;
    const std::vector<double> rpy{ placed.rotation.roll, placed.rotation.pitch,
                                   placed.rotation.yaw };
    return check_triple(declared, key, "rpy", rpy);
}

std::string origin_key(const std::string &owner, const std::string &part, std::size_t at)
{
    const std::string base = "origin." + owner + "." + part;
    return at == 0 ? base : base + "." + std::to_string(at);
}

int check_placed(const std::vector<oracle::row> &rows, const meios::link<double> &one,
                 std::size_t &compared)
{
    for(std::size_t at = 0; at < one.visuals.size(); ++at)
        if(const int rc = check_origin(rows, origin_key(one.name, "visual", at),
                                       one.visuals[at].origin, compared))
            return rc;
    for(std::size_t at = 0; at < one.collisions.size(); ++at)
        if(const int rc = check_origin(rows, origin_key(one.name, "collision", at),
                                       one.collisions[at].origin, compared))
            return rc;
    return 0;
}

int check_origins(const std::vector<oracle::row> &rows, const meios::model<double> &robot)
{
    std::size_t compared = 0;
    for(const meios::link<double> &one : robot.links)
        if(const int rc = check_placed(rows, one, compared))
            return rc;
    const std::size_t recorded = rows_under(rows, "origin.");
    if(compared != recorded)
        return refuse_fact("origin row count", std::to_string(recorded), std::to_string(compared));
    return 0;
}

}

int check_assets(const meios::model<double> &robot, const std::vector<oracle::row> &rows)
{
    if(const int rc = check_meshes(rows, robot))
        return rc;
    return check_origins(rows, robot);
}

}
