#ifndef HPP_GUARD_MEIOS_XACRO_MACRO_TABLES_H
#define HPP_GUARD_MEIOS_XACRO_MACRO_TABLES_H

#include "meios/xacro/detail/table_chain.h"

#include <pugixml.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <functional>
#include <string_view>

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

struct macro_table
{
    std::optional<std::size_t> parent;
    std::map<std::string, macro_def, std::less<>> macros;
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
    void define(std::string name, macro_def def);

    // A copy, held by the call for its duration: the reference clones the definition before the
    // call's own children run, so a redefinition of the running macro does not reach the call.
    std::optional<macro_def> find(std::string_view name) const;

private:
    std::size_t m_cursor;
    std::vector<std::unique_ptr<macro_table>> m_tables;
};

}

#endif
