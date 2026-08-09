#ifndef HPP_GUARD_MEIOS_XACRO_VALUE_H
#define HPP_GUARD_MEIOS_XACRO_VALUE_H

#include "meios/xacro/detail/value_node.h"

#include <cmath>
#include <memory>
#include <ranges>
#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <utility>
#include <variant>
#include <optional>
#include <string_view>

namespace meios
{

enum class value_kind
{
    null,
    boolean,
    integer,
    real,
    string,
    sequence,
    mapping,
};

// Immutable after construction, so a copy shares its collection storage instead of
// duplicating it; that is what lets a mapping cross a property, a macro argument and a
// nested scope by value. The variant's alternatives are declared in value_kind's order,
// so the kind is the held alternative's index.
class value
{
public:
    value() : m_from_yaml(false), m_data() {}
    explicit value(bool flag) : m_from_yaml(false), m_data(flag) {}
    explicit value(std::int64_t number) : m_from_yaml(false), m_data(number) {}
    explicit value(std::string text) : m_from_yaml(false), m_data(std::move(text)) {}

    // A character pointer converts to bool ahead of std::string, which would silently
    // turn an authored literal into a boolean.
    value(const char *) = delete;

    using entry = std::pair<std::string, value>;

    static std::optional<value> make_real(double number);
    static value make_sequence(std::vector<value> items);
    static value make_mapping(std::vector<entry> entries);

    value_kind kind() const { return static_cast<value_kind>(m_data.index()); }

    bool from_yaml() const { return m_from_yaml; }

    value with_yaml_origin() const;

    std::optional<bool> boolean() const { return held<bool>(); }
    std::optional<double> real() const { return held<double>(); }
    std::optional<std::int64_t> integer() const { return held<std::int64_t>(); }
    std::optional<std::string> text() const { return held<std::string>(); }

    std::size_t size() const;
    std::optional<value> at(std::size_t index) const;
    std::optional<value> at(std::string_view key) const;
    std::optional<std::string> key_at(std::size_t index) const;

    bool operator==(const value &other) const;

private:
    using sequence_ref = std::shared_ptr<const detail::sequence_node>;
    using mapping_ref = std::shared_ptr<const detail::mapping_node>;

    bool m_from_yaml;
    std::variant<std::monostate, bool, std::int64_t, double, std::string, sequence_ref,
                 mapping_ref>
        m_data;

    explicit value(double number) : m_from_yaml(false), m_data(number) {}
    explicit value(sequence_ref node) : m_from_yaml(false), m_data(std::move(node)) {}
    explicit value(mapping_ref node) : m_from_yaml(false), m_data(std::move(node)) {}

    template <typename T>
    std::optional<T> held() const
    {
        const T *stored = std::get_if<T>(&m_data);
        return stored == nullptr ? std::nullopt : std::optional<T>(*stored);
    }

    static std::optional<value> copied(const value *found);

    template <typename Node>
    const Node *node_or_null() const
    {
        const std::shared_ptr<const Node> *held = std::get_if<std::shared_ptr<const Node>>(&m_data);
        return held == nullptr ? nullptr : held->get();
    }
};

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
