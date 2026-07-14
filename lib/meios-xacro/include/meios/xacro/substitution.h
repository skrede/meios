#ifndef HPP_GUARD_MEIOS_XACRO_SUBSTITUTION_H
#define HPP_GUARD_MEIOS_XACRO_SUBSTITUTION_H

#include "meios/xacro/eval_scope.h"

#include "meios/diagnostic/log_sink.h"

#include <string>
#include <filesystem>
#include <string_view>

namespace meios
{

class source_stack;

struct substitution
{
    bool ok;
    std::string text;
};

substitution substitute(std::string_view raw, const eval_scope &scope, source_stack &sources,
                        const std::filesystem::path &document, log_sink &log);

}

#endif
