#include "macro_bind.h"
#include "literal_read.h"
#include "dispatch_view.h"
#include "namespace_lift.h"
#include "structural_detail.h"

#include "meios/xacro/structural.h"

#include "meios/xacro/detail/scope_read.h"

#include "meios/diagnostic/diagnostic_code.h"

#include <pugixml.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

namespace
{

bool bind_literal(expand_ctx &ctx, pugi::xml_node call, std::string_view raw,
                  const std::filesystem::path &document, value &bound)
{
    bool ok = true;
    const std::string authored = strip_authored_markers(std::string(raw));
    bound = substitute_attr_value(ctx, call, authored, document, ok);
    return ok;
}

// A `^` default inherits the enclosing value, read as a literal again as any bound value is;
// `^|fallback` inherits it or the fallback text; a bare `^` with no inherited value is a loud
// error, never literal.
bool bind_default(expand_ctx &ctx, pugi::xml_node call, const std::string &name,
                  const std::string &def, const std::filesystem::path &document, value &bound)
{
    if(def != "^" && def.rfind("^|", 0) != 0)
        return bind_literal(ctx, call, def, document, bound);
    const name_read inherited = read_name(ctx.scope, name);
    if(inherited.failed())
        return false;
    if(inherited.bound)
    {
        bound = read_literal(*inherited.bound);
        return true;
    }
    if(def.rfind("^|", 0) == 0)
        return bind_literal(ctx, call, std::string_view(def).substr(2), document, bound);
    std::string message = "macro parameter '" + name + "' inherits no value and has no fallback";
    if(ctx.scope.arguments().contains(name))
        message += " — '" + name + "' is an argument, which a parameter does not inherit; read it with $(arg " + name + ")";
    return fail(ctx, call, diagnostic_code::xacro_structural_error, message);
}

std::string call_to(const macro_def &def)
{
    return std::string("call to macro '") + def.body.attribute("name").value() + "' ";
}

std::vector<pugi::xml_node> element_children(pugi::xml_node root)
{
    std::vector<pugi::xml_node> kids;
    for(pugi::xml_node child : root.children())
        if(child.type() == pugi::node_element)
            kids.push_back(child);
    return kids;
}

std::string block_spelling(const macro_def &def, std::size_t i)
{
    return (def.block_children[i] ? "**" : "*") + def.block_params[i];
}

// Every attribute is checked and evaluated in document order, so the first refusal reported is the
// first one written. An attribute spelled like a block parameter, or a namespace declaration, names
// no parameter an attribute can set.
bool bind_attributes(expand_ctx &ctx, const macro_def &def, const dispatch_view &view, const std::filesystem::path &document,
                     call_binding &bound, std::vector<bool> &supplied)
{
    const pugi::xml_node call = view.node;
    for(pugi::xml_attribute attr : call.attributes())
    {
        if(consumed(view, attr.name()))
            continue;
        const auto at = std::find(def.params.begin(), def.params.end(), std::string_view(attr.name()));
        if(at == def.params.end())
            return fail(ctx, call, diagnostic_code::xacro_structural_error, call_to(def) + "sets '" + attr.name() + "', which names no parameter an attribute can set");
        const std::size_t i = static_cast<std::size_t>(at - def.params.begin());
        if(!bind_literal(ctx, call, attr.value(), document, bound.values[i]))
            return false;
        supplied[i] = true;
    }
    return true;
}

// The whole call node is evaluated in the caller's own macros and symbols before a single block is
// captured, so a block argument is evaluated whether or not the receiving macro ever inserts it,
// and an insert the caller wrote resolves against the caller's blocks. The callee's table binds
// what that walk produced once binding succeeded. A block parameter left without an element refuses
// here, before any default is read.
bool bind_blocks(expand_ctx &ctx, const macro_def &def, pugi::xml_node call,
                 const std::filesystem::path &document, std::vector<bound_block> &blocks)
{
    const emit_origin origin = ctx.origins.empty() ? emit_origin{ document, {} } : ctx.origins.back();
    pugi::xml_node root = ctx.park().append_child("blocks");
    const lift_target dropped(ctx, {});
    if(!process_children(ctx, call, root, document))
        return false;
    const std::vector<pugi::xml_node> kids = element_children(root);
    if(kids.size() > def.block_params.size())
        return fail(ctx, call, diagnostic_code::xacro_structural_error, call_to(def) + "passes element <" + kids[def.block_params.size()].name() + ">, which no block parameter takes");
    if(kids.size() < def.block_params.size())
        return fail(ctx, call, diagnostic_code::xacro_structural_error, call_to(def) + "leaves block parameter '" + block_spelling(def, kids.size()) + "' without an element");
    for(std::size_t i = 0; i < kids.size(); ++i)
    {
        ctx.blocks.push_back(parked_block{ kids[i], def.block_children[i], origin });
        blocks.push_back(bound_block{ block_spelling(def, i), ctx.blocks.size() - 1 });
    }
    return true;
}

// One default is kept per parameter name: the last declaration of the name written with one sets
// it, an empty one clearing it, and a declaration written without one leaves it alone.
std::optional<std::string> named_default(const macro_def &def, const std::string &name)
{
    std::optional<std::string> last;
    for(std::size_t i = 0; i < def.params.size(); ++i)
        if(def.writes_default[i] && def.params[i] == name)
            last = def.defaults[i];
    return last;
}

bool bind_defaults(expand_ctx &ctx, const macro_def &def, pugi::xml_node call, const std::filesystem::path &document,
                   call_binding &bound, std::vector<bool> &supplied)
{
    for(std::size_t i = 0; i < def.params.size(); ++i)
    {
        if(supplied[i])
            continue;
        const std::optional<std::string> written = named_default(def, def.params[i]);
        if(!written)
            continue;
        if(!bind_default(ctx, call, def.params[i], *written, document, bound.values[i]))
            return false;
        supplied[i] = true;
    }
    return true;
}

bool require_supplied(expand_ctx &ctx, const macro_def &def, pugi::xml_node call, const std::vector<bool> &supplied)
{
    for(std::size_t i = 0; i < def.params.size(); ++i)
        if(!supplied[i])
            return fail(ctx, call, diagnostic_code::xacro_structural_error,
                        "macro instantiation is missing required parameter '" + def.params[i] + '\'');
    return true;
}

}

// Every argument is evaluated in the caller's own symbol table before any of them is bound into the
// table the body alone reads. Binding as it went would let one argument read a sibling's new
// value -- which a description does write, passing both `ee_id="${ee_id}_${ee_color}"` and a
// second argument naming `ee_id`, where the two spellings mean different things.
bool bind_call(expand_ctx &ctx, const macro_def &def, const dispatch_view &view, const std::filesystem::path &document, call_binding &bound)
{
    const pugi::xml_node call = view.node;
    std::vector<bool> supplied(def.params.size(), false);
    bound.values.resize(def.params.size());
    return bind_attributes(ctx, def, view, document, bound, supplied)
        && bind_blocks(ctx, def, call, document, bound.blocks)
        && bind_defaults(ctx, def, call, document, bound, supplied)
        && require_supplied(ctx, def, call, supplied);
}

}
