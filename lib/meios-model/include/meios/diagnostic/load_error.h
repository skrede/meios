#ifndef HPP_GUARD_MEIOS_MODEL_DIAGNOSTIC_LOAD_ERROR_H
#define HPP_GUARD_MEIOS_MODEL_DIAGNOSTIC_LOAD_ERROR_H

#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"

#include <string>

namespace meios
{

struct load_error
{
    source_location loc;
    std::string message;
    diagnostic_code code;
};

}

#endif
