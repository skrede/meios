#ifndef HPP_GUARD_MEIOS_XACRO_TABLE_CHAIN_H
#define HPP_GUARD_MEIOS_XACRO_TABLE_CHAIN_H

#include "meios/xacro/detail/name_table.h"

#include "meios/xacro/value.h"

#include <memory>
#include <vector>
#include <cstddef>
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
            , m_tables()
    {
        m_tables.push_back(std::make_unique<name_table>(std::nullopt));
    }

    std::size_t cursor() const
    {
        return m_cursor;
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

    std::optional<value> lookup(std::string_view name) const
    {
        std::optional<std::size_t> at = holder(name);
        if(!at)
            return std::nullopt;
        return m_tables[*at]->find(name);
    }

private:
    std::size_t m_cursor;
    std::vector<std::unique_ptr<name_table>> m_tables;
};

}

}

#endif
