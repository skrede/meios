#include "scope_chain.h"
#include "reference_names.h"
#include "structural_detail.h"

#include "meios/xacro/eval_scope.h"

#include "meios/diagnostic/level.h"

#include <pugixml.hpp>

#include <string>
#include <cstddef>
#include <optional>
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
