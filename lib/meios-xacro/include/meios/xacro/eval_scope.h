#ifndef HPP_GUARD_MEIOS_XACRO_EVAL_SCOPE_H
#define HPP_GUARD_MEIOS_XACRO_EVAL_SCOPE_H

#include "meios/xacro/value.h"

#include <string>
#include <variant>
#include <optional>
#include <string_view>
#include <unordered_map>

namespace meios
{

using binding = std::variant<value, std::string>;

class eval_scope
{
public:
    eval_scope() : m_bindings() {}

    void set(std::string_view name, binding entry)
    {
        m_bindings.insert_or_assign(std::string(name), std::move(entry));
    }

    std::optional<binding> lookup(std::string_view name) const
    {
        std::unordered_map<std::string, binding>::const_iterator it = m_bindings.find(std::string(name));
        if(it == m_bindings.end())
            return std::nullopt;
        return it->second;
    }

    bool contains(std::string_view name) const
    {
        return m_bindings.find(std::string(name)) != m_bindings.end();
    }

private:
    std::unordered_map<std::string, binding> m_bindings;
};

}

#endif
