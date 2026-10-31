#ifndef HPP_GUARD_MEIOS_XACRO_NAME_TABLE_H
#define HPP_GUARD_MEIOS_XACRO_NAME_TABLE_H

#include "meios/xacro/detail/scope_entry.h"

#include <string>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace meios
{

namespace detail
{

// One of the reference's symbol tables: the names a document, one macro call, or one include
// namespace bound itself, and the table a read falls through to when a name is not among them.
// The parent is an index into the owning chain, absent only for the document table.
class name_table
{
public:
    name_table(std::uint64_t serial, std::optional<std::size_t> parent, bool space = false)
            : m_space(space)
            , m_serial(serial)
            , m_parent(parent)
            , m_entries()
    {
    }

    std::uint64_t serial() const
    {
        return m_serial;
    }

    std::optional<std::size_t> parent() const
    {
        return m_parent;
    }

    bool is_namespace() const
    {
        return m_space;
    }

    void set(std::string_view name, scope_entry entry)
    {
        m_entries.insert_or_assign(std::string(name), std::move(entry));
    }

    bool contains(std::string_view name) const
    {
        return m_entries.find(std::string(name)) != m_entries.end();
    }

    const scope_entry *find(std::string_view name) const
    {
        std::unordered_map<std::string, scope_entry>::const_iterator it = m_entries.find(std::string(name));
        if(it == m_entries.end())
            return nullptr;
        return &it->second;
    }

    void erase(std::string_view name)
    {
        m_entries.erase(std::string(name));
    }

    template<typename Predicate>
    void erase_if(Predicate predicate)
    {
        std::erase_if(m_entries, [&predicate](const std::pair<const std::string, scope_entry> &entry) { return predicate(entry.second); });
    }

private:
    bool m_space;
    std::uint64_t m_serial;
    std::optional<std::size_t> m_parent;
    std::unordered_map<std::string, scope_entry> m_entries;
};

}

}

#endif
