#ifndef HPP_GUARD_MEIOS_XACRO_INVOCATION_FRAME_H
#define HPP_GUARD_MEIOS_XACRO_INVOCATION_FRAME_H

#include "meios/xacro/eval_scope.h"

#include "meios/xacro/detail/table_chain.h"

#include <cstddef>

namespace meios::detail
{

// Opens a macro call's own symbol table over the caller's and drops it, with every table opened
// beneath it, on whichever path the call returns, as the reference binds each call in a fresh
// Table that dies with the call. The caller's table as parent is the reference's dynamic scoping.
class invocation_frame
{
public:
    explicit invocation_frame(eval_scope &scope);
    invocation_frame(const invocation_frame &) = delete;
    invocation_frame &operator=(const invocation_frame &) = delete;
    ~invocation_frame();

    std::size_t table() const { return m_table; }

private:
    table_mark m_mark;
    eval_scope &m_scope;
    std::size_t m_table;
};

}

#endif
