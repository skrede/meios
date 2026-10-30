#ifndef HPP_GUARD_MEIOS_XACRO_SCOPE_CHAIN_H
#define HPP_GUARD_MEIOS_XACRO_SCOPE_CHAIN_H

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"

#include "meios/xacro/detail/scope_entry.h"
#include "meios/xacro/detail/table_chain.h"

#include <string>
#include <cstddef>
#include <cstdint>
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

    static std::size_t enter(eval_scope &scope, std::size_t table)
    {
        return scope.m_chain.enter(table);
    }

    static std::size_t open_namespace(eval_scope &scope, std::string_view name)
    {
        return scope.m_chain.open_namespace(name);
    }

    static void define(eval_scope &scope, std::size_t table, std::string_view name, value entry)
    {
        scope.m_chain.table(table).set(name, std::move(entry));
    }

    static void define_deferred(eval_scope &scope, std::size_t table, std::string_view name,
                                std::string text, std::size_t origin)
    {
        const std::uint64_t generation = scope.m_chain.generation();
        scope.m_chain.table(table).set(name, deferred_text{ std::move(text), origin, generation });
    }

    static void install(eval_scope &scope, deferred_resolve resolve, deferred_cycle cycle)
    {
        scope.m_chain.install(std::move(resolve), std::move(cycle));
    }

    static void uninstall(eval_scope &scope)
    {
        scope.m_chain.uninstall();
    }
};

}

#endif
