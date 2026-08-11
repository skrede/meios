#ifndef HPP_GUARD_MEIOS_XACRO_VALUE_NODE_H
#define HPP_GUARD_MEIOS_XACRO_VALUE_NODE_H

#include <string>
#include <vector>
#include <cstddef>
#include <string_view>

namespace meios
{

class value;

namespace detail
{

// Both nodes hold their elements as a vector of an incomplete value, completed in every
// translation unit that includes value.h; a mapping keeps its keys in a vector beside them
// rather than in pairs, because a standard container of an incomplete type is only
// sanctioned for the container itself. Neither node exposes a mutator, so a value sharing
// one cannot be changed through the copy that shares it, and every lookup answers with a
// pointer because absence is a real outcome and value is not complete here.
// Every member reaching into that vector is defined in value_node_access.h rather than inline:
// these are ordinary classes, so an inline body is compiled at the closing brace, where indexing
// a vector of an incomplete type compiles only as a GNU extension and libc++ rejects it.
class sequence_node
{
public:
    explicit sequence_node(std::vector<value> items);

    std::size_t size() const;

    const value *at(std::size_t index) const;

private:
    std::vector<value> m_items;
};

class mapping_node
{
public:
    mapping_node(std::vector<std::string> keys, std::vector<value> items);

    std::size_t size() const { return m_keys.size(); }

    const value *at(std::size_t index) const;

    const std::string *key_at(std::size_t index) const
    {
        return index < m_keys.size() ? &m_keys[index] : nullptr;
    }

    const value *find(std::string_view key) const;

private:
    std::vector<std::string> m_keys;
    std::vector<value> m_items;
};

}

}

#endif
