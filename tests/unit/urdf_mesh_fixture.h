#ifndef HPP_GUARD_MEIOS_UNIT_URDF_MESH_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_URDF_MESH_FIXTURE_H

#include <meios/io.h>
#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/xacro.h>

#include <string>
#include <fstream>
#include <sstream>
#include <variant>
#include <filesystem>

namespace
{

inline std::string slurp(const std::string &name)
{
    const std::filesystem::path path = std::filesystem::path{MEIOS_URDF_FIXTURE_DIR} / name;
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

inline meios::tree<double> parse_document(const std::string &document, meios::source_stack &sources, meios::missing_asset miss, meios::log_sink &log)
{
    meios::core_evaluator eval;
    meios::parse_context ctx{sources, eval, log, miss, meios::topology_policy::fail, meios::material_policy::warn, meios::strictness::fail, {}};
    meios::pod_recorder<meios::tree<double>> rec(log, meios::topology_policy::fail);
    meios::basic_parser<meios::urdf_reader> parser(ctx);
    parser.parse(slurp(document), rec);
    return rec.result();
}

inline const meios::mesh<double> &first_mesh(const meios::tree<double> &robot)
{
    return std::get<meios::mesh<double>>(robot.links.at(0).visuals.at(0).geom.shape);
}

inline std::filesystem::path mesh_fixture()
{
    return std::filesystem::path{MEIOS_URDF_FIXTURE_DIR} / "package_mesh.urdf";
}

}

#endif
