#ifndef HPP_GUARD_MEIOS_XACRO_SCOPE_READ_H
#define HPP_GUARD_MEIOS_XACRO_SCOPE_READ_H

#include "meios/xacro/detail/scope_entry.h"

#include "meios/xacro/eval_scope.h"
#include "meios/xacro/scope_namespace.h"
#include "meios/xacro/eval_failure_kind.h"

#include <cstddef>
#include <optional>
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
    return scope.m_chain.read(scope.m_chain.cursor(), name, carried);
}

// A member the namespace lacks is read from the tables it falls through to, and a deferred one
// evaluates in the table that holds it, as the reference's namespace forwards a dot to its lookup.
inline name_read read_member(const eval_scope &scope, const scope_namespace &space, std::string_view name,
                             std::size_t carried)
{
    if(!scope.m_chain.live(space.m_entry))
        return name_read{ std::nullopt, eval_failure_kind::none, std::nullopt };
    return scope.m_chain.read(space.m_entry.table, name, carried);
}

}

#endif
