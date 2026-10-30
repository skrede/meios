#ifndef HPP_GUARD_MEIOS_XACRO_TABLE_CHAIN_H
#define HPP_GUARD_MEIOS_XACRO_TABLE_CHAIN_H

#include "meios/xacro/detail/name_table.h"
#include "meios/xacro/detail/scope_entry.h"

#include "meios/xacro/value.h"
#include "meios/xacro/eval_failure_kind.h"

#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <variant>
#include <optional>
#include <functional>
#include <string_view>

namespace meios
{

namespace detail
{

struct table_mark
{
    std::size_t size;
    std::size_t cursor;
};

struct resolve_step
{
    std::size_t table;
    std::string name;
    std::size_t origin;
};

using deferred_resolve = std::function<name_read(const deferred_text &)>;
using deferred_cycle = std::function<eval_failure_kind(const std::vector<resolve_step> &, std::string_view)>;

// Tables live in an arena and name their parent by index, so truncating to a mark drops a call's
// table together with every table opened beneath it and leaves no parent dangling. Index 0 is the
// document table and is never dropped.
class table_chain
{
public:
    table_chain()
            : m_cursor(0)
            , m_generation(0)
            , m_resolving()
            , m_tables()
            , m_cycle_cb()
            , m_resolve_cb()
    {
        m_tables.push_back(std::make_unique<name_table>(std::nullopt));
    }

    std::size_t cursor() const
    {
        return m_cursor;
    }

    std::uint64_t generation() const
    {
        return m_generation;
    }

    table_mark mark() const
    {
        return table_mark{ m_tables.size(), m_cursor };
    }

    std::size_t push(std::size_t parent)
    {
        m_tables.push_back(std::make_unique<name_table>(parent));
        m_cursor = m_tables.size() - 1;
        return m_cursor;
    }

    void pop_to(table_mark at)
    {
        m_tables.resize(at.size);
        m_cursor = at.cursor;
    }

    name_table &table(std::size_t index)
    {
        return *m_tables[index];
    }

    const name_table &table(std::size_t index) const
    {
        return *m_tables[index];
    }

    std::optional<std::size_t> holder(std::string_view name) const
    {
        for(std::optional<std::size_t> at = m_cursor; at; at = m_tables[*at]->parent())
            if(m_tables[*at]->contains(name))
                return at;
        return std::nullopt;
    }

    void install(deferred_resolve resolve, deferred_cycle cycle)
    {
        ++m_generation;
        m_resolve_cb = std::move(resolve);
        m_cycle_cb = std::move(cycle);
    }

    void uninstall()
    {
        m_resolve_cb = nullptr;
        m_cycle_cb = nullptr;
        m_resolving.clear();
        pop_to(table_mark{ 1, 0 });
    }

    name_read read(std::string_view name)
    {
        const std::optional<std::size_t> at = holder(name);
        if(!at)
            return name_read{ std::nullopt, eval_failure_kind::none };
        const scope_entry &entry = *m_tables[*at]->find(name);
        if(const value *bound = std::get_if<value>(&entry))
            return name_read{ *bound, eval_failure_kind::none };
        if(const failed_read *failed = std::get_if<failed_read>(&entry))
            return name_read{ std::nullopt, failed->kind };
        return resolve(*at, name, std::get<deferred_text>(entry));
    }

private:
    std::size_t m_cursor;
    std::uint64_t m_generation;
    std::vector<resolve_step> m_resolving;
    std::vector<std::unique_ptr<name_table>> m_tables;
    deferred_cycle m_cycle_cb;
    deferred_resolve m_resolve_cb;

    bool resolving(std::size_t table, std::string_view name) const
    {
        for(const resolve_step &step : m_resolving)
            if(step.table == table && step.name == name)
                return true;
        return false;
    }

    // The text is evaluated with its own table current, as the reference evaluates it against the
    // table holding the key, and the answer replaces the text there whether it is a value or a
    // refusal. A text written by an expansion that has since returned reads as nothing.
    name_read resolve(std::size_t table, std::string_view name, deferred_text deferred)
    {
        if(!m_resolve_cb || deferred.generation != m_generation)
            return name_read{ std::nullopt, eval_failure_kind::none };
        if(resolving(table, name))
            return name_read{ std::nullopt, m_cycle_cb(m_resolving, name) };
        m_resolving.push_back(resolve_step{ table, std::string(name), deferred.origin });
        const std::size_t reader = std::exchange(m_cursor, table);
        name_read answer = m_resolve_cb(deferred);
        m_cursor = reader;
        m_resolving.pop_back();
        if(answer.bound)
            m_tables[table]->set(name, *answer.bound);
        else
            m_tables[table]->set(name, failed_read{ answer.failure });
        return answer;
    }
};

}

}

#endif
