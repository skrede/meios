#ifndef HPP_GUARD_MEIOS_MODEL_RECORDS_GEOMETRY_H
#define HPP_GUARD_MEIOS_MODEL_RECORDS_GEOMETRY_H

#include "meios/math/vector3.h"

#include <string>
#include <variant>
#include <optional>

namespace meios
{

template <typename Scalar = double>
struct mesh
{
    std::string filename;
    vector3<Scalar> scale;
    std::optional<std::string> resolved_path;
};

template <typename Scalar = double>
struct box
{
    vector3<Scalar> size;
};

template <typename Scalar = double>
struct cylinder
{
    Scalar radius;
    Scalar length;
};

template <typename Scalar = double>
struct sphere
{
    Scalar radius;
};

template <typename Scalar = double>
struct geometry
{
    std::variant<mesh<Scalar>, box<Scalar>, cylinder<Scalar>, sphere<Scalar>> shape;
};

}

#endif
