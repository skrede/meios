#ifndef HPP_GUARD_MEIOS_XACRO_STRUCTURAL_DETAIL_H
#define HPP_GUARD_MEIOS_XACRO_STRUCTURAL_DETAIL_H

#include "expand_context.h"

#include <pugixml.hpp>

#include <string>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

void seed_declared_args(expand_ctx &ctx, pugi::xml_node node);

std::optional<std::size_t> attr_dom_index(pugi::xml_node in, std::string_view name);

std::string substitute_attr(expand_ctx &ctx, pugi::xml_node in, std::string_view raw,
                            const std::filesystem::path &document, bool &ok,
                            std::optional<std::size_t> attr_index = std::nullopt);

value substitute_attr_value(expand_ctx &ctx, pugi::xml_node in, std::string_view raw,
                            const std::filesystem::path &document, bool &ok,
                            std::optional<std::size_t> attr_index = std::nullopt);

value substitute_arg_value(expand_ctx &ctx, pugi::xml_node in, std::string_view raw,
                           const std::filesystem::path &document, bool &ok,
                           std::optional<std::size_t> attr_index = std::nullopt);

std::string strip_container_marker(std::string text);

bool process_children(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                      const std::filesystem::path &document);
bool process_node(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                  const std::filesystem::path &document);
bool emit_element(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                  const std::filesystem::path &document);
bool insert_block(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out);

bool expand_include(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                    const std::filesystem::path &document);

struct include_scope
{
    std::size_t symbols;
    std::size_t macros;
};

// The tables an include's file expands in: a namespace the include names, opened and bound in the
// current tables, or the current tables themselves. A cleared ok is a namespace that refused.
include_scope include_table(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document, bool &ok);

// Makes an include's tables current for as long as its file expands, and the including ones current
// again on whichever path the expansion returns.
class include_tables
{
public:
    include_tables(expand_ctx &ctx, include_scope tables);
    include_tables(const include_tables &) = delete;
    include_tables &operator=(const include_tables &) = delete;
    ~include_tables();

private:
    expand_ctx &m_ctx;
    include_scope m_including;
};

bool define_property(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document);
bool declare_arg(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document);

bool parse_params(std::string_view spec, macro_def &def);

bool define_macro(expand_ctx &ctx, pugi::xml_node in);
bool instantiate_macro(expand_ctx &ctx, const resolved_macro &found, pugi::xml_node call,
                       pugi::xml_node out, const std::filesystem::path &document);

}

#endif
