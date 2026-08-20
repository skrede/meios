#ifndef HPP_GUARD_MEIOS_XACRO_VALUE_H
#define HPP_GUARD_MEIOS_XACRO_VALUE_H

#include "meios/xacro/detail/value_node.h"

#include <memory>
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

}

// Included here, not at the top: these definitions index a vector of value, so they compile
// only where value is complete.
#include "meios/xacro/detail/value_node_access.h"

// Likewise at the bottom: value's own out-of-line members need value complete.
#include "meios/xacro/detail/value_access.h"

#endif
