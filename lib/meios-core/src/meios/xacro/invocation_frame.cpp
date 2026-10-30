#include "scope_chain.h"
#include "invocation_frame.h"

#include "meios/xacro/eval_scope.h"

namespace meios::detail
{

invocation_frame::invocation_frame(eval_scope &scope)
    : m_mark(chain_access::mark(scope)), m_scope(scope),
      m_table(chain_access::push(scope, chain_access::cursor(scope)))
{
}

invocation_frame::~invocation_frame()
{
    chain_access::pop_to(m_scope, m_mark);
}

}
