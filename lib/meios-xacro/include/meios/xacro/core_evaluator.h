#ifndef HPP_GUARD_MEIOS_XACRO_CORE_EVALUATOR_H
#define HPP_GUARD_MEIOS_XACRO_CORE_EVALUATOR_H

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"

#include "meios/diagnostic/log_sink.h"

#include <string_view>

namespace meios
{

class core_evaluator
{
public:
    core_evaluator() : m_failed(false) {}

    value eval(std::string_view expression, const eval_scope &scope, log_sink &log);

    bool failed() const { return m_failed; }

private:
    bool m_failed;
};

}

#endif
