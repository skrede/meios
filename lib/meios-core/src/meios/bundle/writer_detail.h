#ifndef HPP_GUARD_MEIOS_CORE_BUNDLE_WRITER_DETAIL_H
#define HPP_GUARD_MEIOS_CORE_BUNDLE_WRITER_DETAIL_H

#include "meios/bundle/urdf_writer.h"

#include "meios/records/link.h"
#include "meios/records/joint.h"
#include "meios/records/material.h"
#include "meios/records/geometry.h"

#include "meios/math/vector3.h"
#include "meios/math/transform.h"
#include "meios/math/rotations.h"

#include <pugixml.hpp>

#include <string>
#include <vector>
#include <string_view>

namespace meios::detail
{

bool xml10_char_ok(std::string_view text);

std::string xyz_string(const vector3<double> &v);

std::string rpy_string(const rpy<double> &r);

void append_origin(pugi::xml_node parent, const transform<double> &tf);

void append_material(pugi::xml_node parent, const material<double> &mat,
                     std::vector<reference_record> &refs);

void append_material_ref(pugi::xml_node parent, const std::string &name);

void append_geometry(pugi::xml_node parent, const geometry<double> &geom,
                     std::vector<reference_record> &refs);

void append_link(pugi::xml_node parent, const link<double> &node,
                 std::vector<reference_record> &refs);

void append_joint(pugi::xml_node parent, const joint<double> &edge);

}

#endif
