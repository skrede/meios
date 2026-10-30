#include "macro_tables.h"

#include "meios/xacro/detail/table_chain.h"

#include <map>
#include <memory>
#include <string>
#include <cstddef>
#include <utility>
#include <optional>
#include <functional>
#include <string_view>

namespace meios::detail
{

macro_chain::macro_chain()
    : m_cursor(0), m_tables()
{
    m_tables.push_back(std::make_unique<macro_table>(macro_table{ std::nullopt, {} }));
}

std::size_t macro_chain::cursor() const
{
    return m_cursor;
}

table_mark macro_chain::mark() const
{
    return table_mark{ m_tables.size(), m_cursor };
}

std::size_t macro_chain::push(std::size_t parent)
{
    m_tables.push_back(std::make_unique<macro_table>(macro_table{ parent, {} }));
    m_cursor = m_tables.size() - 1;
    return m_cursor;
}

void macro_chain::pop_to(table_mark at)
{
    m_tables.resize(at.size);
    m_cursor = at.cursor;
}

void macro_chain::define(std::string name, macro_def def)
{
    m_tables[m_cursor]->macros.insert_or_assign(std::move(name), std::move(def));
}

std::optional<macro_def> macro_chain::find(std::string_view name) const
{
    for(std::optional<std::size_t> at = m_cursor; at; at = m_tables[*at]->parent)
    {
        const std::map<std::string, macro_def, std::less<>> &table = m_tables[*at]->macros;
        if(const auto found = table.find(name); found != table.end())
            return found->second;
    }
    return std::nullopt;
}

}
