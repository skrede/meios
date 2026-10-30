#ifndef HPP_GUARD_MEIOS_XACRO_MACRO_TABLES_H
#define HPP_GUARD_MEIOS_XACRO_MACRO_TABLES_H

#include "meios/xacro/detail/table_chain.h"

#include <pugixml.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <variant>
#include <optional>
#include <filesystem>
#include <functional>
#include <string_view>

namespace meios
{
class eval_scope;
}

namespace meios::detail
{

struct emit_origin
{
    std::filesystem::path path;
    std::string_view text;
};

struct macro_def
{
    std::vector<std::string> params;
    std::vector<std::optional<std::string>> defaults;
    std::vector<bool> writes_default;
    std::vector<std::string> block_params;
    std::vector<bool> block_children;
    pugi::xml_node body;
    emit_origin origin;
};

struct macro_namespace
{
    std::size_t table;
};

// The reference keeps a macro and an include namespace under one name in a single table.
using macro_entry = std::variant<macro_def, macro_namespace>;

struct macro_table
{
    std::optional<std::size_t> parent;
    std::map<std::string, macro_entry, std::less<>> macros;
};

// A copy of the definition, held by the call for its duration: the reference clones it before the
// call's own children run, so a redefinition of the running macro does not reach the call. The two
// tables are the parents of the call's own, the caller's for a plain name and the namespace's for
// a dotted one.
struct resolved_macro
{
    macro_def def;
    std::size_t macros;
    std::size_t symbols;
};

// The reference's macro tables chain the way its symbol tables do: a macro call opens one whose
// parent is the caller's, so a macro its body defines is dropped with the call while every macro
// the caller could name stays reachable. Index 0 is the document table and is never dropped.
class macro_chain
{
public:
    macro_chain();

    std::size_t cursor() const;
    table_mark mark() const;
    std::size_t push(std::size_t parent);
    void pop_to(table_mark at);
    std::size_t enter(std::size_t table);
    void define(std::string name, macro_def def);

    // The namespace's table has no parent, so a macro its file defines cannot call one of the
    // including file. It is bound in the current table, and the cursor stays where it was.
    std::size_t open_namespace(std::string name);

    const macro_entry *find(std::size_t from, std::string_view name) const;

private:
    std::size_t m_cursor;
    std::vector<std::unique_ptr<macro_table>> m_tables;
};

// The reference tries the name as written first and only then splits it at its dots, walking each
// namespace through the macro chain and the symbol chain together; a head bound as a namespace in
// only one of them names no macro.
std::optional<resolved_macro> resolve_macro(const macro_chain &macros, const eval_scope &scope, std::string_view name);

}

#endif
