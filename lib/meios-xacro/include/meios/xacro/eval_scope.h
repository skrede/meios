#ifndef HPP_GUARD_MEIOS_XACRO_EVAL_SCOPE_H
#define HPP_GUARD_MEIOS_XACRO_EVAL_SCOPE_H

#include "meios/xacro/value.h"
#include "meios/xacro/text_resource_loader.h"

#include <string>
#include <utility>
#include <variant>
#include <optional>
#include <filesystem>
#include <string_view>
#include <unordered_map>

namespace meios
{

using binding = std::variant<value, std::string>;

class eval_scope
{
public:
    eval_scope() : m_load_text_cb(), m_active_document(), m_bindings() {}

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

    void erase(std::string_view name)
    {
        m_bindings.erase(std::string(name));
    }

    void install_text_loader(text_resource_loader loader)
    {
        m_load_text_cb = std::move(loader);
    }

    // Returns the document that was active, so the expansion layer restores it on the
    // way out of an include without the scope publishing an accessor for it.
    std::filesystem::path set_active_document(std::filesystem::path document)
    {
        return std::exchange(m_active_document, std::move(document));
    }

    std::optional<std::string> load_text(std::string_view spec) const
    {
        if(!m_load_text_cb.valid())
            return std::nullopt;
        return m_load_text_cb(spec, m_active_document);
    }

private:
    text_resource_loader m_load_text_cb;
    std::filesystem::path m_active_document;
    std::unordered_map<std::string, binding> m_bindings;
};

}

#endif
