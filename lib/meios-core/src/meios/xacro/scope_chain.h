#ifndef HPP_GUARD_MEIOS_XACRO_SCOPE_CHAIN_H
#define HPP_GUARD_MEIOS_XACRO_SCOPE_CHAIN_H

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"

#include "meios/xacro/detail/table_chain.h"

#include <cstddef>
#include <utility>
#include <optional>
#include <string_view>

namespace meios::detail
{

// The expansion layer's only door to the tables behind a scope: a consumer holding an eval_scope
// reads and writes names but cannot open or drop a table.
struct chain_access
{
    static constexpr std::size_t document_table = 0;

    static std::size_t cursor(const eval_scope &scope)
    {
        return scope.m_chain.cursor();
    }

    static std::optional<std::size_t> parent(const eval_scope &scope, std::size_t table)
    {
        return scope.m_chain.table(table).parent();
    }

    static table_mark mark(const eval_scope &scope)
    {
        return scope.m_chain.mark();
    }

    static std::size_t push(eval_scope &scope, std::size_t parent)
    {
        return scope.m_chain.push(parent);
    }

    static void pop_to(eval_scope &scope, table_mark at)
    {
        scope.m_chain.pop_to(at);
    }

    static void define(eval_scope &scope, std::size_t table, std::string_view name, value entry)
    {
        scope.m_chain.table(table).set(name, std::move(entry));
    }
};

}

#endif
