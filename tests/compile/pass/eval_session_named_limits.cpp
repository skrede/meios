// The negative assertion next door names a core-private header, and a case that cannot find its
// header fails to compile for the wrong reason while still satisfying WILL_FAIL. This control
// compiles the same header in ALL, so a broken include path turns the build red instead.
#include "meios/xacro/eval_session.h"

int main()
{
    const meios::evaluator_limits ceilings;
    meios::detail::eval_session session(ceilings);
    return session.span_depth == 0 ? 0 : 1;
}
