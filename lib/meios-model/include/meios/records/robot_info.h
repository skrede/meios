#ifndef HPP_GUARD_MEIOS_MODEL_RECORDS_ROBOT_INFO_H
#define HPP_GUARD_MEIOS_MODEL_RECORDS_ROBOT_INFO_H

#include "meios/records/extension.h"

#include <string>
#include <vector>

namespace meios
{

struct robot_info
{
    std::string name;
    std::vector<extension> extensions;
};

}

#endif
