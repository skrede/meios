#include "scope_chain.h"
#include "macro_tables.h"
#include "invocation_frame.h"

#include "meios/xacro/eval_scope.h"

namespace meios::detail
{

invocation_frame::invocation_frame(eval_scope &scope, std::size_t symbols, macro_chain &macros, std::size_t macro_parent)
    : m_mark(chain_access::mark(scope)), m_scope(scope), m_table(chain_access::push(scope, symbols)), m_macros(macros),
      m_macro_mark(macros.mark())
{
    m_macros.push(macro_parent);
}

invocation_frame::~invocation_frame()
{
    m_macros.pop_to(m_macro_mark);
    chain_access::pop_to(m_scope, m_mark);
}

}
