#ifndef HPP_GUARD_MEIOS_XACRO_INVOCATION_FRAME_H
#define HPP_GUARD_MEIOS_XACRO_INVOCATION_FRAME_H

#include "macro_tables.h"

#include "meios/xacro/eval_scope.h"

#include "meios/xacro/detail/table_chain.h"

#include <cstddef>

namespace meios::detail
{

// Opens a macro call's own symbol and macro tables over the caller's and drops them, with every
// table opened beneath them, on whichever path the call returns, as the reference binds each call
// in a fresh pair of Tables that die with the call. The caller's tables as parents are the
// reference's dynamic scoping.
class invocation_frame
{
public:
    invocation_frame(eval_scope &scope, macro_chain &macros);
    invocation_frame(const invocation_frame &) = delete;
    invocation_frame &operator=(const invocation_frame &) = delete;
    ~invocation_frame();

    std::size_t table() const { return m_table; }

private:
    table_mark m_mark;
    eval_scope &m_scope;
    std::size_t m_table;
    macro_chain &m_macros;
    table_mark m_macro_mark;
};

}

#endif
