#ifndef HPP_GUARD_MEIOS_XACRO_EVAL_SCOPE_H
#define HPP_GUARD_MEIOS_XACRO_EVAL_SCOPE_H

#include "meios/xacro/value.h"
#include "meios/xacro/evaluator_limits.h"
#include "meios/xacro/yaml_parser_handle.h"
#include "meios/xacro/text_resource_loader.h"

#include "meios/xacro/detail/table_chain.h"

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

namespace detail
{
struct chain_access;
}

// A write and an erase act on the current table; a read walks from it outward through the tables
// of the enclosing calls to the document's, so the innermost binding of a name is the one read.
class eval_scope
{
    friend struct detail::chain_access;

public:
    eval_scope()
            : m_load_text_cb()
            , m_parse_yaml_cb()
            , m_chain()
            , m_active_document()
            , m_arguments()
    {
    }

    void set(std::string_view name, value entry)
    {
        m_chain.table(m_chain.cursor()).set(name, std::move(entry));
    }

    std::optional<value> lookup(std::string_view name) const
    {
        return m_chain.lookup(name);
    }

    bool contains(std::string_view name) const
    {
        return m_chain.holder(name).has_value();
    }

    void erase(std::string_view name)
    {
        m_chain.table(m_chain.cursor()).erase(name);
    }

    // $(arg n) reads this table and an expression reads the other, so a property of an
    // argument's name is a second binding beside the argument, never a write to it.
    void bind_argument(std::string_view name, value entry)
    {
        m_arguments.insert_or_assign(std::string(name), std::move(entry));
    }

    std::optional<value> argument(std::string_view name) const
    {
        std::unordered_map<std::string, value>::const_iterator it = m_arguments.find(std::string(name));
        if(it == m_arguments.end())
            return std::nullopt;
        return it->second;
    }

    bool has_argument(std::string_view name) const
    {
        return m_arguments.find(std::string(name)) != m_arguments.end();
    }

    void unbind_argument(std::string_view name)
    {
        m_arguments.erase(std::string(name));
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

    // The active document is the file being processed: the expansion layer sets it on the way
    // into an include and restores the returned one on the way out, and a macro call leaves it
    // alone, so inside a macro body it is the file that called the macro.
    std::filesystem::path set_active_document(std::filesystem::path document)
    {
        return std::exchange(m_active_document, std::move(document));
    }

    const std::filesystem::path &active_document() const
    {
        return m_active_document;
    }

    text_resource_result load_text(std::string_view spec) const
    {
        if(!m_load_text_cb.valid())
            return unexpected<text_resource_failure>(text_resource_failure::unreachable);
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
    detail::table_chain m_chain;
    std::filesystem::path m_active_document;
    std::unordered_map<std::string, value> m_arguments;
};

}

#endif
