#ifndef HPP_GUARD_MEIOS_MODEL_RECORDS_LOOP_CONSTRAINT_H
#define HPP_GUARD_MEIOS_MODEL_RECORDS_LOOP_CONSTRAINT_H

#include "meios/math/transform.h"

#include <string>

namespace meios
{

template <typename Scalar = double>
struct loop_constraint
{
    std::string parent;
    std::string child;
    transform<Scalar> origin;
};

}

#endif
