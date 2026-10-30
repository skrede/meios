#include "scope_chain.h"
#include "macro_tables.h"

#include "meios/xacro/eval_scope.h"

#include "meios/xacro/detail/table_chain.h"

#include <map>
#include <memory>
#include <string>
#include <cstddef>
#include <utility>
#include <variant>
#include <optional>
#include <functional>
#include <string_view>

namespace meios::detail
{

namespace
{

std::optional<std::size_t> macro_space(const macro_chain &macros, std::size_t from, std::string_view name)
{
    const macro_entry *found = macros.find(from, name);
    const macro_namespace *space = found ? std::get_if<macro_namespace>(found) : nullptr;
    return space ? std::optional<std::size_t>(space->table) : std::nullopt;
}

std::optional<resolved_macro> resolve_in(const macro_chain &macros, std::size_t at, std::size_t symbols, std::string_view name)
{
    const macro_entry *found = macros.find(at, name);
    const macro_def *def = found ? std::get_if<macro_def>(found) : nullptr;
    if(!def)
        return std::nullopt;
    return resolved_macro{ *def, at, symbols };
}

}

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

std::size_t macro_chain::enter(std::size_t table)
{
    return std::exchange(m_cursor, table);
}

void macro_chain::define(std::string name, macro_def def)
{
    m_tables[m_cursor]->macros.insert_or_assign(std::move(name), macro_entry(std::move(def)));
}

std::size_t macro_chain::open_namespace(std::string name)
{
    m_tables.push_back(std::make_unique<macro_table>(macro_table{ std::nullopt, {} }));
    const std::size_t opened = m_tables.size() - 1;
    m_tables[m_cursor]->macros.insert_or_assign(std::move(name), macro_entry(macro_namespace{ opened }));
    return opened;
}

const macro_entry *macro_chain::find(std::size_t from, std::string_view name) const
{
    for(std::optional<std::size_t> at = from; at; at = m_tables[*at]->parent)
    {
        const std::map<std::string, macro_entry, std::less<>> &table = m_tables[*at]->macros;
        if(const auto found = table.find(name); found != table.end())
            return &found->second;
    }
    return nullptr;
}

std::optional<resolved_macro> resolve_macro(const macro_chain &macros, const eval_scope &scope, std::string_view name)
{
    std::size_t at = macros.cursor();
    std::size_t symbols = chain_access::cursor(scope);
    if(macros.find(at, name))
        return resolve_in(macros, at, symbols, name);
    for(std::size_t dot = name.find('.'); dot != std::string_view::npos; dot = name.find('.'))
    {
        const std::optional<std::size_t> space = macro_space(macros, at, name.substr(0, dot));
        const std::optional<std::size_t> names = chain_access::namespace_table(scope, symbols, name.substr(0, dot));
        if(!space || !names)
            return std::nullopt;
        at = *space;
        symbols = *names;
        name.remove_prefix(dot + 1);
    }
    return resolve_in(macros, at, symbols, name);
}

}
