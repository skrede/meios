#include "scope_chain.h"
#include "structural_detail.h"

#include "meios/xacro/eval_scope.h"

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
std::size_t parent_table(const eval_scope &scope, std::size_t current)
{
    std::optional<std::size_t> target = chain_access::parent(scope, current);
    if(!chain_access::is_namespace(scope, current))
        while(target && chain_access::is_namespace(scope, *target))
            target = chain_access::parent(scope, *target);
    return target.value_or(chain_access::document_table);
}

}

// The reference picks the table a property is stored in by the scope attribute alone: the
// document's for "global", the caller's for "parent", and the current one for any other value, an
// unknown one included. A document-level "parent", which the reference stores into its table of
// built-ins, lands in the document table.
std::size_t property_target(const expand_ctx &ctx, std::string_view scope_attr)
{
    const std::size_t current = chain_access::cursor(ctx.scope);
    if(scope_attr == "global")
        return chain_access::document_table;
    if(scope_attr == "parent")
        return parent_table(ctx.scope, current);
    return current;
}

bool remove_property(expand_ctx &ctx, std::string_view name)
{
    if(!ctx.scope.contains(name))
        return false;
    chain_access::remove(ctx.scope, name);
    return true;
}

}
