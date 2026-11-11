#include "macro_bind.h"
#include "scope_chain.h"
#include "namespace_lift.h"
#include "invocation_frame.h"
#include "structural_detail.h"

#include "meios/xacro/eval_scope.h"
#include "meios/xacro/structural.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <pugixml.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <optional>
#include <filesystem>

namespace meios::detail
{

namespace
{

// The definition element is cloned and the clone evaluated before the wrapper is discarded, so
// every parameter default is evaluated once per instantiation, in the invocation's own scope,
// whether or not the caller supplied that parameter, and the result is thrown away. A default
// naming an unbound property therefore refuses there even where the call supplied the parameter --
// measured against xacro 2.1.1, which renders `^` and `^|text` unchanged in the same position, so
// what runs here is the substitution of the authored text and not a second reading of the
// inheritance arms. It runs before the definition's origin is pushed, so a refusal is located at
// the call.
bool substitute_defaults(expand_ctx &ctx, const macro_def &def, pugi::xml_node call,
                         const std::filesystem::path &document)
{
    bool ok = true;
    for(const std::optional<std::string> &written : def.defaults)
    {
        if(!written)
            continue;
        substitute_attr_value(ctx, call, strip_authored_markers(*written), document, ok);
        if(!ok)
            return false;
    }
    return true;
}

// The whole name is checked before a prefix is removed, so `xacro:a.b` refuses as dotted.
bool valid_macro_name(expand_ctx &ctx, pugi::xml_node in, const std::string &name)
{
    if(name == "call")
        return fail(ctx, in, diagnostic_code::xacro_structural_error, "a macro cannot be named 'call', which xacro:call takes");
    if(name.find('.') != std::string::npos)
        return fail(ctx, in, diagnostic_code::xacro_structural_error, "macro name '" + name + "' holds a '.', which separates a namespace from a name");
    return true;
}

}

bool define_macro(expand_ctx &ctx, pugi::xml_node in)
{
    std::string name = in.attribute("name").value();
    if(!valid_macro_name(ctx, in, name))
        return false;
    if(name.starts_with("xacro:"))
    {
        ctx.log.log(level::warn, locate(ctx, in), "macro name '" + name + "' is defined as '" + name.substr(6) + '\'');
        name.erase(0, 6);
    }
    macro_def def;
    // Such a list refuses when the definition is read, called or not.
    if(!parse_params(in.attribute("params").value(), def))
        return fail(ctx, in, diagnostic_code::xacro_structural_error, "macro '" + name + "' has a params attribute that holds only whitespace");
    def.body = in;
    if(!ctx.origins.empty())
        def.origin = ctx.origins.back();
    ctx.macros.define(name, std::move(def));
    return true;
}

bool instantiate_macro(expand_ctx &ctx, const resolved_macro &found, const dispatch_view &view,
                       pugi::xml_node out, const std::filesystem::path &document)
{
    const macro_def &def      = found.def;
    const pugi::xml_node call = view.node;
    call_binding bound;
    if(!ctx.charge_work(call) || !bind_call(ctx, def, view, document, bound))
        return false;
    const invocation_frame frame(ctx.scope, found.symbols, ctx.macros, found.macros);
    for(std::size_t i = 0; i < def.params.size(); ++i)
    {
        warn_global_symbol(ctx, call, def.params[i]);
        chain_access::define(ctx.scope, frame.table(), def.params[i], bound.values[i]);
    }
    for(const bound_block &block : bound.blocks)
        chain_access::define_block(ctx.scope, frame.table(), block.key, block.block);
    if(!substitute_defaults(ctx, def, call, document))
        return false;
    ctx.origins.push_back(def.origin);
    const bool ok = lift_namespaces(ctx, def.body, document) && process_children(ctx, def.body, out, document);
    ctx.origins.pop_back();
    return ok;
}

}
