#ifndef HPP_GUARD_MEIOS_XACRO_SUBSTITUTION_H
#define HPP_GUARD_MEIOS_XACRO_SUBSTITUTION_H

#include "meios/xacro/eval_scope.h"
#include "meios/xacro/eval_policy.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/source_location.h"

#include <memory>
#include <string>
#include <filesystem>
#include <string_view>

namespace meios
{

class source_stack;
class evaluator_handle;

struct substitution
{
    bool ok;
    std::string text;
};

substitution substitute(std::string_view raw, const eval_scope &scope, source_stack &sources,
                        const std::filesystem::path &document, eval_policy policy,
                        const std::shared_ptr<evaluator_handle> &backend, log_sink &log,
                        const source_location &at);

substitution substitute(std::string_view raw, const eval_scope &scope, source_stack &sources,
                        const std::filesystem::path &document, eval_policy policy,
                        const std::shared_ptr<evaluator_handle> &backend, log_sink &log);

substitution substitute(std::string_view raw, const eval_scope &scope, source_stack &sources,
                        const std::filesystem::path &document, log_sink &log);

}

#endif
