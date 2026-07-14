#include "lexer.h"
#include "eval_parser.h"

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/core_evaluator.h"

#include "meios/diagnostic/log_sink.h"

#include <vector>
#include <string_view>

namespace meios
{

value core_evaluator::eval(std::string_view expression, const eval_scope &scope, log_sink &log)
{
    std::vector<detail::token> tokens = detail::tokenize(expression);
    detail::parser state(tokens, scope, log);
    value result = detail::parse_ternary(state);
    if(state.ok && !state.at(detail::token_kind::end))
        result = state.fail("unexpected trailing tokens in expression");
    m_failed = !state.ok;
    m_kind = state.failure;
    return result;
}

}
