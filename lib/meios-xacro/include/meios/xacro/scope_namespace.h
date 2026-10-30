#ifndef HPP_GUARD_MEIOS_XACRO_SCOPE_NAMESPACE_H
#define HPP_GUARD_MEIOS_XACRO_SCOPE_NAMESPACE_H

#include "meios/xacro/detail/scope_entry.h"

#include <cstddef>
#include <string_view>

namespace meios
{

class eval_scope;
class scope_namespace;

namespace detail
{
inline name_read read_member(const eval_scope &scope, const scope_namespace &space, std::string_view name,
                             std::size_t carried = 0);
}

// An include namespace, which an expression reads through a dot. It names a table of the expansion
// that opened it, so once that expansion has returned every read through it is absent.
class scope_namespace
{
    friend class eval_scope;
    friend detail::name_read detail::read_member(const eval_scope &scope, const scope_namespace &space,
                                                 std::string_view name, std::size_t carried);

private:
    explicit scope_namespace(detail::namespace_entry entry)
            : m_entry(entry)
    {
    }

    detail::namespace_entry m_entry;
};

}

#endif
