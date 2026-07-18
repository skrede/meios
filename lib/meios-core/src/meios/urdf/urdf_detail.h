#ifndef HPP_GUARD_MEIOS_URDF_URDF_DETAIL_H
#define HPP_GUARD_MEIOS_URDF_URDF_DETAIL_H

#include "meios/urdf/parse_context.h"

#include "meios/records/link.h"
#include "meios/records/joint.h"
#include "meios/records/geometry.h"
#include "meios/records/material.h"

#include "meios/math/vector3.h"
#include "meios/math/transform.h"

#include "meios/diagnostic/source_location.h"

#include <pugixml.hpp>

#include <string>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>
#include <unordered_map>

namespace meios::detail
{

using material_table = std::unordered_map<std::string, material<double>>;

int offset_to_line(std::string_view text, std::ptrdiff_t offset);

source_location offset_location(std::string_view text, std::ptrdiff_t offset,
                                const std::filesystem::path &file);

source_location node_location(pugi::xml_node node, std::string_view text,
                              const std::filesystem::path &file);

bool read_finite(std::string_view text, double &out, parse_context &ctx,
                 const source_location &loc, std::string_view field);

vector3<double> read_vec3(std::string_view text, parse_context &ctx, const source_location &loc,
                          std::string_view field);

transform<double> read_transform(pugi::xml_node origin, parse_context &ctx,
                                 const source_location &loc);

material<double> extract_material(pugi::xml_node node, std::string_view text,
                                  const std::filesystem::path &file, parse_context &ctx);

link<double> extract_link(pugi::xml_node node, std::string_view text,
                          const std::filesystem::path &file, parse_context &ctx,
                          const material_table &materials);

joint<double> extract_joint(pugi::xml_node node, std::string_view text,
                            const std::filesystem::path &file, parse_context &ctx);

geometry<double> read_geometry(pugi::xml_node node, parse_context &ctx, const source_location &loc);

void resolve_mesh(mesh<double> &shape, parse_context &ctx, const source_location &loc);

bool run_strictness(pugi::xml_node document, std::string_view text,
                    const std::filesystem::path &file, parse_context &ctx);

}

#endif
