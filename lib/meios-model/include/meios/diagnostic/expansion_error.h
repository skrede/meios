#ifndef HPP_GUARD_MEIOS_MODEL_DIAGNOSTIC_EXPANSION_ERROR_H
#define HPP_GUARD_MEIOS_MODEL_DIAGNOSTIC_EXPANSION_ERROR_H

#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"
#include "meios/diagnostic/operation_failure.h"

#include <string>
#include <optional>

namespace meios
{

struct expansion_error
{
    source_location loc;
    std::string message;
    diagnostic_code code;
    std::optional<operation_failure> cause;
};

}

#endif
