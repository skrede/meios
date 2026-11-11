#ifndef HPP_GUARD_MEIOS_XACRO_STRUCTURAL_DETAIL_H
#define HPP_GUARD_MEIOS_XACRO_STRUCTURAL_DETAIL_H

#include "dispatch_view.h"
#include "expand_context.h"

#include <pugixml.hpp>

#include <string>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

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

// The argument reading, under the fail policy whatever policy the document expands under: a
// condition decides which subtree exists, so it is never left half-expanded.
value substitute_condition_value(expand_ctx &ctx, pugi::xml_node in, std::string_view raw,
                                 const std::filesystem::path &document, bool &ok,
                                 std::optional<std::size_t> attr_index = std::nullopt);

std::string strip_container_marker(std::string text);

bool process_children(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                      const std::filesystem::path &document);
bool process_node(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                  const std::filesystem::path &document);
bool dispatch_element(expand_ctx &ctx, const dispatch_view &view, pugi::xml_node out,
                      const std::filesystem::path &document);
bool emit_element(expand_ctx &ctx, pugi::xml_node in, const std::string &name, pugi::xml_node out,
                  const std::filesystem::path &document);
bool insert_block(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                  const std::filesystem::path &document);

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
// Absent where the name refused, which it does before anything else is read.
std::optional<std::string> property_name(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document);
// Absent for a document-level parent scope, which refuses once the value is evaluated.
std::optional<std::size_t> property_target(const expand_ctx &ctx, std::string_view scope_attr);
bool property_defined(const expand_ctx &ctx, std::string_view name);
// False where the name is not defined, which leaves the property to be defined as written.
bool remove_property(expand_ctx &ctx, pugi::xml_node in, std::string_view name);
void warn_global_symbol(expand_ctx &ctx, pugi::xml_node in, std::string_view name);
bool declare_arg(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document);

bool parse_params(std::string_view spec, macro_def &def);

bool define_macro(expand_ctx &ctx, pugi::xml_node in);
bool instantiate_macro(expand_ctx &ctx, const resolved_macro &found, const dispatch_view &call,
                       pugi::xml_node out, const std::filesystem::path &document);
// The macro call a name in xacro's namespace makes when no construct takes it; xacro:call names its
// macro in an attribute instead.
bool call_macro(expand_ctx &ctx, const dispatch_view &view, pugi::xml_node out, const std::filesystem::path &document);
bool rename_element(expand_ctx &ctx, const dispatch_view &view, pugi::xml_node out, const std::filesystem::path &document);
// The text an attribute of the dispatched node evaluates to; empty for an attribute a rename consumed.
std::string evaluated_text(expand_ctx &ctx, const dispatch_view &view, std::string_view attr, const std::filesystem::path &document, bool &ok);

// Makes an element the one whose output the walk fills for as long as it lives.
class emitting_scope
{
public:
    emitting_scope(expand_ctx &ctx, emission current);
    emitting_scope(const emitting_scope &)            = delete;
    emitting_scope &operator=(const emitting_scope &) = delete;
    ~emitting_scope();

private:
    expand_ctx &m_ctx;
};

bool set_attribute(expand_ctx &ctx, const dispatch_view &view, pugi::xml_node out, const std::filesystem::path &document);
// An insertion of a parked block's content alone sets the attributes placed on the block on out.
void carry_placed(expand_ctx &ctx, pugi::xml_node parked, pugi::xml_node out);
// An insertion of a parked block whole notes the attributes placed on it away from their parent.
void note_placed(expand_ctx &ctx, pugi::xml_node parked);

}

#endif
