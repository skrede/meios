#ifndef HPP_GUARD_MEIOS_MODEL_DIAGNOSTIC_LOAD_ERROR_H
#define HPP_GUARD_MEIOS_MODEL_DIAGNOSTIC_LOAD_ERROR_H

#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"
#include "meios/diagnostic/captured_diagnostic.h"

#include <string>
#include <vector>

namespace meios
{

struct load_error
{
    source_location loc;
    std::string message;
    diagnostic_code code;
    std::vector<captured_diagnostic> diagnostics;
};

}

#endif
