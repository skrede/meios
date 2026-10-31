#ifndef HPP_GUARD_MEIOS_XACRO_TABLE_CHAIN_H
#define HPP_GUARD_MEIOS_XACRO_TABLE_CHAIN_H

#include "meios/xacro/detail/name_table.h"
#include "meios/xacro/detail/scope_entry.h"
#include "meios/xacro/detail/deferred_resolver.h"

#include "meios/xacro/value.h"
#include "meios/xacro/eval_failure_kind.h"

#include <memory>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <variant>
#include <optional>
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

// Tables live in an arena and name their parent by index, so truncating to a mark drops a call's
// table together with every table opened beneath it and leaves no parent dangling. Index 0 is the
// document table and is never dropped.
class table_chain
{
public:
    table_chain()
            : m_cursor(0)
            , m_generation(0)
            , m_resolver()
            , m_tables()
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

    std::size_t enter(std::size_t table)
    {
        return std::exchange(m_cursor, table);
    }

    // The namespace's table falls through to the current one and is bound there under its name, so
    // a second namespace of that name replaces the first. The cursor stays where it was.
    std::size_t open_namespace(std::string_view name)
    {
        m_tables.push_back(std::make_unique<name_table>(m_cursor, true));
        m_tables[m_cursor]->set(name, namespace_entry{ m_tables.size() - 1, m_generation });
        return m_tables.size() - 1;
    }

    bool live(const namespace_entry &space) const
    {
        return space.generation == m_generation && space.table < m_tables.size();
    }

    std::optional<std::size_t> holder(std::size_t from, std::string_view name) const
    {
        for(std::optional<std::size_t> at = from; at; at = m_tables[*at]->parent())
            if(m_tables[*at]->contains(name))
                return at;
        return std::nullopt;
    }

    std::optional<namespace_entry> namespace_at(std::size_t from, std::string_view name) const
    {
        const std::optional<std::size_t> at = holder(from, name);
        const namespace_entry *found = at ? std::get_if<namespace_entry>(m_tables[*at]->find(name)) : nullptr;
        if(!found || !live(*found))
            return std::nullopt;
        return *found;
    }

    void install(deferred_resolve resolve, deferred_cycle cycle)
    {
        ++m_generation;
        m_resolver.attach(std::move(resolve), std::move(cycle));
    }

    void uninstall()
    {
        m_resolver.detach();
        pop_to(table_mark{ 1, 0 });
    }

    name_read read(std::size_t from, std::string_view name, std::size_t carried = 0)
    {
        const std::optional<std::size_t> at = holder(from, name);
        if(!at)
            return name_read{ std::nullopt, eval_failure_kind::none, std::nullopt };
        const scope_entry &entry = *m_tables[*at]->find(name);
        if(const value *bound = std::get_if<value>(&entry))
            return name_read{ *bound, eval_failure_kind::none, std::nullopt };
        if(const failed_read *failed = std::get_if<failed_read>(&entry))
            return name_read{ std::nullopt, failed->kind, std::nullopt };
        if(const deferred_text *deferred = std::get_if<deferred_text>(&entry))
            return resolve(*at, name, *deferred, carried);
        const namespace_entry *space = std::get_if<namespace_entry>(&entry);
        return name_read{ std::nullopt, eval_failure_kind::none, space && live(*space) ? std::optional(*space) : std::nullopt };
    }

    name_read read_in(const namespace_entry &space, std::string_view name, std::size_t carried = 0)
    {
        if(!live(space))
            return name_read{ std::nullopt, eval_failure_kind::none, std::nullopt };
        return read(space.table, name, carried);
    }

private:
    std::size_t m_cursor;
    std::uint64_t m_generation;
    deferred_resolver m_resolver;
    std::vector<std::unique_ptr<name_table>> m_tables;

    // The text is evaluated with its own table current, as the reference evaluates it against the
    // table holding the key, and the answer replaces the text there whether it is a value or a
    // refusal. A text written by an expansion that has since returned reads as nothing.
    name_read resolve(std::size_t table, std::string_view name, deferred_text deferred, std::size_t carried)
    {
        if(!m_resolver.attached() || deferred.generation != m_generation)
            return name_read{ std::nullopt, eval_failure_kind::none, std::nullopt };
        if(const std::optional<eval_failure_kind> refused = m_resolver.cycle(table, name))
            return name_read{ std::nullopt, *refused, std::nullopt };
        const std::size_t reader = std::exchange(m_cursor, table);
        name_read answer = m_resolver.evaluate(table, name, deferred, carried);
        m_cursor = reader;
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
