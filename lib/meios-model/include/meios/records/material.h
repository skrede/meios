#ifndef HPP_GUARD_MEIOS_MODEL_RECORDS_MATERIAL_H
#define HPP_GUARD_MEIOS_MODEL_RECORDS_MATERIAL_H

#include <string>
#include <optional>

namespace meios
{

template <typename Scalar = double>
struct rgba
{
    Scalar r;
    Scalar g;
    Scalar b;
    Scalar a;
};

template <typename Scalar = double>
struct material
{
    std::string name;
    std::optional<rgba<Scalar>> color;
    std::optional<std::string> texture;
};

}

#endif
