#ifndef HPP_GUARD_MEIOS_XACRO_SCOPE_READ_H
#define HPP_GUARD_MEIOS_XACRO_SCOPE_READ_H

#include "meios/xacro/detail/scope_entry.h"

#include "meios/xacro/eval_scope.h"

#include <string_view>

namespace meios::detail
{

// The read an evaluator's own name lookup needs: unlike lookup, it tells a name nothing binds
// from one whose deferred text refused, so a read site can propagate a refusal that already
// reported itself instead of reporting the name as undefined beside it.
inline name_read read_name(const eval_scope &scope, std::string_view name)
{
    return scope.m_chain.read(name);
}

}

#endif
