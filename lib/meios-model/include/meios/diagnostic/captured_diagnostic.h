#ifndef HPP_GUARD_MEIOS_MODEL_DIAGNOSTIC_CAPTURED_DIAGNOSTIC_H
#define HPP_GUARD_MEIOS_MODEL_DIAGNOSTIC_CAPTURED_DIAGNOSTIC_H

#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"

#include <string>

namespace meios
{

struct captured_diagnostic
{
    diagnostic_code code;
    source_location loc;
    std::string message;
};

}

#endif
