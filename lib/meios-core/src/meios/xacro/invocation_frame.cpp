#include "scope_chain.h"
#include "macro_tables.h"
#include "invocation_frame.h"

#include "meios/xacro/eval_scope.h"

namespace meios::detail
{

invocation_frame::invocation_frame(eval_scope &scope, macro_chain &macros)
    : m_mark(chain_access::mark(scope)), m_scope(scope),
      m_table(chain_access::push(scope, chain_access::cursor(scope))), m_macros(macros), m_macro_mark(macros.mark())
{
    m_macros.push(m_macros.cursor());
}

invocation_frame::~invocation_frame()
{
    m_macros.pop_to(m_macro_mark);
    chain_access::pop_to(m_scope, m_mark);
}

}
