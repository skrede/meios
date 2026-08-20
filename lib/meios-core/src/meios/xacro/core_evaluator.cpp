#include "lexer.h"
#include "eval_parser.h"
#include "eval_session.h"

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/core_evaluator.h"
#include "meios/xacro/evaluator_limits.h"

#include "meios/diagnostic/log_sink.h"

#include <vector>
#include <string_view>

namespace meios
{

value core_evaluator::eval(std::string_view expression, const eval_scope &scope, log_sink &log,
                           const source_location &at)
{
    const evaluator_limits ceilings;
    detail::eval_session session(ceilings);
    return eval(expression, scope, log, at, session);
}

value core_evaluator::eval(std::string_view expression, const eval_scope &scope, log_sink &log,
                           const source_location &at, detail::eval_session &session)
{
    std::vector<detail::token> tokens = detail::tokenize(expression);
    if(!session.charge_tokens(tokens.size(), log, at)
       || !session.admits_expression_tokens(tokens.size(), log, at))
    {
        m_failed = true;
        m_kind = eval_failure_kind::exhausted;
        return value{};
    }
    detail::parser state(tokens, scope, log, session, at);
    value result =
        detail::refuse_malformed_syntax(state) ? value{} : detail::parse_ternary(state);
    if(state.ok && !state.at(detail::token_kind::end))
        result = state.fail("unexpected trailing tokens in expression");
    m_failed = !state.ok;
    m_kind = state.failure;
    return result;
}

}
