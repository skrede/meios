#ifndef HPP_GUARD_MEIOS_XACRO_SCOPE_READ_H
#define HPP_GUARD_MEIOS_XACRO_SCOPE_READ_H

#include "meios/xacro/detail/scope_entry.h"

#include "meios/xacro/eval_scope.h"

#include <cstddef>
#include <string_view>

namespace meios::detail
{

// The read an evaluator's own name lookup needs: unlike lookup, it tells a name nothing binds
// from one whose deferred text refused, so a read site can propagate a refusal that already
// reported itself instead of reporting the name as undefined beside it. A read made from inside a
// parser carries that parser's live nesting, because its frames stay on the stack beneath the
// evaluation a deferred text starts.
inline name_read read_name(const eval_scope &scope, std::string_view name, std::size_t carried)
{
    return scope.m_chain.read(name, carried);
}

}

#endif
