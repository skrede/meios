#ifndef HPP_GUARD_MEIOS_UNIT_SILENT_DEFAULT_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_SILENT_DEFAULT_FIXTURE_H

#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/captured_diagnostic.h>

#include <string>
#include <vector>
#include <string_view>

namespace
{

inline int count_code(const std::vector<meios::captured_diagnostic> &records, meios::diagnostic_code code)
{
    int n = 0;
    for(const meios::captured_diagnostic &record : records)
        if(record.code == code)
            ++n;
    return n;
}

inline bool mentions(const std::vector<meios::captured_diagnostic> &records, std::string_view needle)
{
    for(const meios::captured_diagnostic &record : records)
        if(record.message.find(needle) != std::string::npos)
            return true;
    return false;
}

}

#endif
