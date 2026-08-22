#ifndef HPP_GUARD_MEIOS_URDF_LOAD_RESULT_H
#define HPP_GUARD_MEIOS_URDF_LOAD_RESULT_H

#include "meios/diagnostic/completeness.h"
#include "meios/diagnostic/captured_diagnostic.h"

#include "meios/model/model.h"

#include <vector>

namespace meios
{

struct load_result
{
    model<double> robot;
    std::vector<captured_diagnostic> diagnostics;
    completeness claims;
};

// The two members are spelled exactly as they are in load_result, so a caller that
// gives up the model to a sink of its own does not have to relearn the result.
struct load_summary
{
    std::vector<captured_diagnostic> diagnostics;
    completeness claims;
};

}

#endif
