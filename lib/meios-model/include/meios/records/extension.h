#ifndef HPP_GUARD_MEIOS_MODEL_RECORDS_EXTENSION_H
#define HPP_GUARD_MEIOS_MODEL_RECORDS_EXTENSION_H

#include <string>
#include <vector>
#include <utility>

namespace meios
{

struct extension
{
    std::string element;
    std::string content;
    std::vector<std::pair<std::string, std::string>> attributes;
};

}

#endif
