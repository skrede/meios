#ifndef HPP_GUARD_MEIOS_XACRO_ARGUMENT_TABLE_H
#define HPP_GUARD_MEIOS_XACRO_ARGUMENT_TABLE_H

#include "meios/xacro/value.h"

#include <string>
#include <utility>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace meios
{

class argument_table
{
public:
    argument_table()
            : m_entries()
    {
    }

    void set(std::string_view name, value entry)
    {
        m_entries.insert_or_assign(std::string(name), std::move(entry));
    }

    std::optional<value> lookup(std::string_view name) const
    {
        std::unordered_map<std::string, value>::const_iterator it = m_entries.find(std::string(name));
        if(it == m_entries.end())
            return std::nullopt;
        return it->second;
    }

    bool contains(std::string_view name) const
    {
        return m_entries.find(std::string(name)) != m_entries.end();
    }

    void erase(std::string_view name)
    {
        m_entries.erase(std::string(name));
    }

private:
    std::unordered_map<std::string, value> m_entries;
};

}

#endif
