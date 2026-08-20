#ifndef HPP_GUARD_MEIOS_XACRO_VALUE_ACCESS_H
#define HPP_GUARD_MEIOS_XACRO_VALUE_ACCESS_H

#include "meios/xacro/detail/value_node.h"

#include "meios/xacro/value.h"

#include <cmath>
#include <memory>
#include <ranges>
#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <optional>
#include <string_view>

namespace meios
{

inline std::optional<value> value::copied(const value *found)
{
    return found == nullptr ? std::nullopt : std::optional<value>(*found);
}

inline std::optional<value> value::make_real(double number)
{
    if(!std::isfinite(number))
        return std::nullopt;
    return value(number);
}

inline value value::make_sequence(std::vector<value> items)
{
    return value(std::make_shared<const detail::sequence_node>(std::move(items)));
}

// A repeated key keeps its first position and its last value, the way an authored
// mapping resolves upstream.
inline value value::make_mapping(std::vector<entry> entries)
{
    std::vector<std::string> keys;
    std::vector<value> items;
    for(entry &incoming : entries)
    {
        const std::vector<std::string>::iterator seen = std::ranges::find(keys, incoming.first);
        if(seen == keys.end())
        {
            keys.push_back(std::move(incoming.first));
            items.push_back(std::move(incoming.second));
        }
        else
        {
            items[static_cast<std::size_t>(seen - keys.begin())] = std::move(incoming.second);
        }
    }
    return value(std::make_shared<const detail::mapping_node>(std::move(keys), std::move(items)));
}

inline value value::with_yaml_origin() const
{
    value marked(*this);
    marked.m_from_yaml = true;
    return marked;
}

inline std::size_t value::size() const
{
    if(const detail::sequence_node *items = node_or_null<detail::sequence_node>())
        return items->size();
    if(const detail::mapping_node *table = node_or_null<detail::mapping_node>())
        return table->size();
    return 0;
}

inline std::optional<value> value::at(std::size_t index) const
{
    if(const detail::sequence_node *items = node_or_null<detail::sequence_node>())
        return copied(items->at(index));
    if(const detail::mapping_node *table = node_or_null<detail::mapping_node>())
        return copied(table->at(index));
    return std::nullopt;
}

inline std::optional<value> value::at(std::string_view key) const
{
    const detail::mapping_node *table = node_or_null<detail::mapping_node>();
    return table == nullptr ? std::nullopt : copied(table->find(key));
}

inline std::optional<std::string> value::key_at(std::size_t index) const
{
    const detail::mapping_node *table = node_or_null<detail::mapping_node>();
    const std::string *key = table == nullptr ? nullptr : table->key_at(index);
    return key == nullptr ? std::nullopt : std::optional<std::string>(*key);
}

inline bool value::operator==(const value &other) const
{
    if(kind() != other.kind() || m_from_yaml != other.m_from_yaml)
        return false;
    if(kind() != value_kind::sequence && kind() != value_kind::mapping)
        return m_data == other.m_data;
    if(size() != other.size())
        return false;
    for(std::size_t i = 0; i < size(); ++i)
        if(key_at(i) != other.key_at(i) || at(i) != other.at(i))
            return false;
    return true;
}

}

#endif
