#ifndef HPP_GUARD_MEIOS_TESTS_MODEL_FACTS_H
#define HPP_GUARD_MEIOS_TESTS_MODEL_FACTS_H

#include "fact_rows.h"

#include <meios/model.h>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <vector>
#include <cstddef>
#include <variant>
#include <optional>
#include <algorithm>
#include <filesystem>

namespace facts
{

inline std::string kind_text(meios::joint_kind kind)
{
    switch(kind)
    {
        case meios::joint_kind::revolute:   return "revolute";
        case meios::joint_kind::continuous: return "continuous";
        case meios::joint_kind::prismatic:  return "prismatic";
        case meios::joint_kind::floating:   return "floating";
        case meios::joint_kind::planar:     return "planar";
        case meios::joint_kind::fixed:      return "fixed";
    }
    return "unrecognized";
}

template <typename Item>
inline void check_names(const std::vector<oracle::row> &rows, const std::string &prefix,
                        const std::vector<Item> &items)
{
    CHECK(std::to_string(items.size()) == value_of(rows, prefix + ".count"));
    for(std::size_t at = 0; at < items.size(); ++at)
    {
        const std::string key = prefix + ".name." + std::to_string(at);
        INFO(key << " loaded as " << items[at].name);
        CHECK(items[at].name == value_of(rows, key));
    }
}

inline void check_limit(const std::string &owner, const std::string &recorded,
                        const meios::joint_limits<double> &limits)
{
    const std::map<std::string, double> loaded{ { "lower", limits.lower },
                                                { "upper", limits.upper },
                                                { "effort", limits.effort },
                                                { "velocity", limits.velocity } };
    for(const std::pair<const std::string, std::vector<double>> &field : keyed_numbers(recorded))
    {
        REQUIRE(loaded.count(field.first) == 1);
        REQUIRE(field.second.size() == 1);
        INFO("joint " << owner << " limit " << field.first << ": recorded " << field.second.front()
                      << ", loaded " << loaded.at(field.first));
        CHECK(loaded.at(field.first) == field.second.front());
    }
}

inline void check_joints(const std::vector<oracle::row> &rows, const meios::model<double> &robot)
{
    check_names(rows, "joint", robot.joints);
    for(const meios::joint<double> &one : robot.joints)
    {
        INFO("joint: " << one.name);
        CHECK(kind_text(one.kind) == value_of(rows, "joint.type." + one.name));
        const std::optional<oracle::row> limit = oracle::lookup(rows, "joint.limit." + one.name);
        CHECK(limit.has_value() == one.limits.has_value());
        if(limit.has_value() && one.limits.has_value())
            check_limit(one.name, limit->fields.at(1), *one.limits);
    }
}

inline void note_mesh(const meios::geometry<double> &geom, std::vector<std::string> &seen)
{
    if(!std::holds_alternative<meios::mesh<double>>(geom.shape))
        return;
    const meios::mesh<double> &found = std::get<meios::mesh<double>>(geom.shape);
    INFO("mesh: " << found.filename);
    REQUIRE(found.resolved_path.has_value());
    CHECK(std::filesystem::exists(*found.resolved_path));
    if(std::find(seen.begin(), seen.end(), found.filename) == seen.end())
        seen.push_back(found.filename);
}

// Distinct references in document order, which is the order the record lists them in: within a
// link the description declares its visuals before its collisions.
inline std::vector<std::string> mesh_references(const meios::model<double> &robot)
{
    std::vector<std::string> seen;
    for(const meios::link<double> &one : robot.links)
    {
        for(const meios::visual<double> &part : one.visuals)
            note_mesh(part.geom, seen);
        for(const meios::collision<double> &part : one.collisions)
            note_mesh(part.geom, seen);
    }
    return seen;
}

inline void check_meshes(const std::vector<oracle::row> &rows, const meios::model<double> &robot)
{
    const std::vector<std::string> seen = mesh_references(robot);
    CHECK(seen.size() == rows_under(rows, "mesh.filename."));
    for(std::size_t at = 0; at < seen.size(); ++at)
        CHECK(seen[at] == value_of(rows, "mesh.filename." + std::to_string(at)));
}

inline void check_triple(const std::map<std::string, std::vector<double>> &declared,
                         const std::string &field, const std::vector<double> &loaded)
{
    const std::map<std::string, std::vector<double>>::const_iterator found = declared.find(field);
    if(found == declared.end())
        return;
    INFO("origin field: " << field);
    REQUIRE(found->second.size() == loaded.size());
    for(std::size_t at = 0; at < loaded.size(); ++at)
        CHECK(loaded[at] == found->second[at]);
}

// A part the description places by nothing carries no recorded row, so an absent key is not a
// failure here; check_origins counts what was compared and holds the record to it.
inline std::size_t check_origin(const std::vector<oracle::row> &rows, const std::string &key,
                                const meios::transform<double> &placed)
{
    const std::optional<oracle::row> found = oracle::lookup(rows, key);
    if(!found || found->fields.size() < 2)
        return 0;
    INFO("origin " << key << " recorded as " << found->fields[1]);
    const std::map<std::string, std::vector<double>> declared = keyed_numbers(found->fields[1]);
    check_triple(declared, "xyz",
                 { placed.translation.x, placed.translation.y, placed.translation.z });
    check_triple(declared, "rpy",
                 { placed.rotation.roll, placed.rotation.pitch, placed.rotation.yaw });
    return 1;
}

inline std::string origin_key(const std::string &owner, const std::string &part, std::size_t at)
{
    const std::string base = "origin." + owner + "." + part;
    return at == 0 ? base : base + "." + std::to_string(at);
}

inline void check_origins(const std::vector<oracle::row> &rows, const meios::model<double> &robot)
{
    std::size_t compared = 0;
    for(const meios::link<double> &one : robot.links)
    {
        for(std::size_t at = 0; at < one.visuals.size(); ++at)
            compared += check_origin(rows, origin_key(one.name, "visual", at),
                                     one.visuals[at].origin);
        for(std::size_t at = 0; at < one.collisions.size(); ++at)
            compared += check_origin(rows, origin_key(one.name, "collision", at),
                                     one.collisions[at].origin);
    }
    CHECK(compared == rows_under(rows, "origin."));
}

inline void check_model(const std::vector<oracle::row> &rows, const meios::model<double> &robot)
{
    REQUIRE_FALSE(rows.empty());
    check_names(rows, "link", robot.links);
    check_joints(rows, robot);
    check_meshes(rows, robot);
    check_origins(rows, robot);
}

}

#endif
