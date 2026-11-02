#include "scope_chain.h"
#include "identifier_text.h"
#include "reference_names.h"
#include "structural_detail.h"

#include "meios/xacro/value.h"
#include "meios/xacro/structural.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/value_render.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <pugixml.hpp>

#include <string>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

namespace
{

// The reference skips namespaces only when the current table is not one, which is a macro call's,
// so a macro a namespace holds writes into its caller's table, while a namespaced file's own
// top level writes into the including table.
std::optional<std::size_t> parent_table(const eval_scope &scope, std::size_t current)
{
    std::optional<std::size_t> target = chain_access::parent(scope, current);
    if(!chain_access::is_namespace(scope, current))
        while(target && chain_access::is_namespace(scope, *target))
            target = chain_access::parent(scope, *target);
    return target;
}

bool valid_property_name(expand_ctx &ctx, pugi::xml_node in, const std::string &name)
{
    if(!valid_python_name(name))
        return fail(ctx, in, diagnostic_code::xacro_structural_error, "Property names must be valid python identifiers: " + name);
    if(name.starts_with("__"))
        return fail(ctx, in, diagnostic_code::xacro_structural_error, "Property names must not start with double underscore: " + name);
    return true;
}

}

// The reference requires the name and evaluates it as text before it reads anything else about the
// property, so a single expression keeps its type and a name that is not text cannot key a table;
// the text is then checked as a Python name, and for a leading double underscore.
std::optional<std::string> property_name(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document)
{
    if(!in.attribute("name"))
    {
        fail(ctx, in, diagnostic_code::xacro_structural_error, "xacro:property: missing attribute 'name'");
        return std::nullopt;
    }
    const std::string authored = strip_authored_markers(in.attribute("name").value());
    bool ok                    = true;
    const value evaluated      = substitute_arg_value(ctx, in, authored, document, ok, attr_dom_index(in, "name"));
    if(!ok)
        return std::nullopt;
    const std::optional<std::string> name = evaluated.text();
    if(!name)
        fail(ctx, in, diagnostic_code::xacro_structural_error,
             "xacro:property name=\"" + authored + "\" evaluates to a " + std::string(kind_name(evaluated.kind())) + ", and a property must be named by text");
    if(!name || !valid_property_name(ctx, in, *name))
        return std::nullopt;
    return name;
}

// The reference picks the table a property is stored in by the scope attribute alone: the
// document's for "global", the caller's for "parent", and the current one for any other value, an
// unknown one included. The document table's parent is the reference's table of built-ins, which
// cannot store a name, so a document-level "parent" has no table.
std::optional<std::size_t> property_target(const expand_ctx &ctx, std::string_view scope_attr)
{
    const std::size_t current = chain_access::cursor(ctx.scope);
    if(scope_attr == "global")
        return chain_access::document_table;
    if(scope_attr == "parent")
        return parent_table(ctx.scope, current);
    return current;
}

// The reference's table of built-ins sits beneath every table, so each of its names is defined.
bool property_defined(const expand_ctx &ctx, std::string_view name)
{
    return ctx.scope.contains(name) || listed(root_symbols, name);
}

// A built-in's bindings are erased from the tables and the built-in itself stays, as in the
// reference.
bool remove_property(expand_ctx &ctx, pugi::xml_node in, std::string_view name)
{
    if(!property_defined(ctx, name))
        return false;
    chain_access::remove(ctx.scope, name);
    if(listed(root_symbols, name))
        ctx.log.log(level::warn, locate(ctx, in), "Cannot remove global symbol: " + std::string(name));
    return true;
}

// The reference warns on every store of a name its table of built-ins holds, into whichever table,
// and the stored name then shadows the built-in. A warning stays uncoded, so it does not retract a
// completeness claim the load still earns.
void warn_global_symbol(expand_ctx &ctx, pugi::xml_node in, std::string_view name)
{
    if(listed(root_symbols, name))
        ctx.log.log(level::warn, locate(ctx, in), "redefining global symbol: " + std::string(name));
}

}
