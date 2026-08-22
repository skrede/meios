#ifndef HPP_GUARD_MEIOS_XACRO_EVAL_SCOPE_H
#define HPP_GUARD_MEIOS_XACRO_EVAL_SCOPE_H

#include "meios/xacro/value.h"
#include "meios/xacro/evaluator_limits.h"
#include "meios/xacro/yaml_parser_handle.h"
#include "meios/xacro/text_resource_loader.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"

#include <memory>
#include <string>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>
#include <unordered_map>

namespace meios
{

class eval_scope
{
public:
    eval_scope()
        : m_load_text_cb(), m_parse_yaml_cb(), m_active_document(), m_bindings()
    {
    }

    void set(std::string_view name, value entry)
    {
        m_bindings.insert_or_assign(std::string(name), std::move(entry));
    }

    std::optional<value> lookup(std::string_view name) const
    {
        std::unordered_map<std::string, value>::const_iterator it = m_bindings.find(std::string(name));
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

    // Installed beside the text loader so a parse is reachable only for bytes the
    // contained loader produced, and so expansion needs no extra parameter to find it.
    void install_yaml_parser(std::shared_ptr<const yaml_parser_handle> parser)
    {
        m_parse_yaml_cb = std::move(parser);
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

    yaml_outcome parse_yaml(std::string_view bytes, const evaluator_limits &limits,
                            evaluator_counters &counters, log_sink &log,
                            const source_location &at) const
    {
        if(!m_parse_yaml_cb || !m_parse_yaml_cb->valid())
        {
            log.log(level::error, diagnostic_code::unsupported_expression, at,
                    "this build resolves no auxiliary document format — use eval-python");
            return yaml_outcome{ std::nullopt, yaml_failure::unavailable };
        }
        return (*m_parse_yaml_cb)(bytes, limits, counters, log, at);
    }

private:
    text_resource_loader m_load_text_cb;
    std::shared_ptr<const yaml_parser_handle> m_parse_yaml_cb;
    std::filesystem::path m_active_document;
    std::unordered_map<std::string, value> m_bindings;
};

}

#endif
