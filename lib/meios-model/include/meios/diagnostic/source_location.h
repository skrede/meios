#ifndef HPP_GUARD_MEIOS_MODEL_DIAGNOSTIC_SOURCE_LOCATION_H
#define HPP_GUARD_MEIOS_MODEL_DIAGNOSTIC_SOURCE_LOCATION_H

#include <string>
#include <filesystem>

namespace meios
{

struct source_location
{
    std::filesystem::path file;
    int line;
    int column;
};

inline std::string to_string(const source_location &location)
{
    return location.file.string() + ':' + std::to_string(location.line) + ':'
         + std::to_string(location.column);
}

}

#endif
