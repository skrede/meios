#include "meios/xacro/eval_session.h"

int main()
{
    meios::detail::eval_session session{ meios::evaluator_limits{} };
    return session.span_depth == 0 ? 0 : 1;
}
