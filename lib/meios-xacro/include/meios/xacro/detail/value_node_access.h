#ifndef HPP_GUARD_MEIOS_XACRO_VALUE_NODE_ACCESS_H
#define HPP_GUARD_MEIOS_XACRO_VALUE_NODE_ACCESS_H

#include "meios/xacro/detail/value_node.h"

#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <string_view>

namespace meios
{

namespace detail
{

inline sequence_node::sequence_node(std::vector<value> items) : m_items(std::move(items)) {}

inline std::size_t sequence_node::size() const
{
    return m_items.size();
}

inline const value *sequence_node::at(std::size_t index) const
{
    return index < m_items.size() ? &m_items[index] : nullptr;
}

inline mapping_node::mapping_node(std::vector<std::string> keys, std::vector<value> items)
    : m_keys(std::move(keys)), m_items(std::move(items))
{
}

inline const value *mapping_node::at(std::size_t index) const
{
    return index < m_items.size() ? &m_items[index] : nullptr;
}

inline const value *mapping_node::find(std::string_view key) const
{
    for(std::size_t i = 0; i < m_keys.size(); ++i)
        if(m_keys[i] == key)
            return &m_items[i];
    return nullptr;
}

}

}

#endif
