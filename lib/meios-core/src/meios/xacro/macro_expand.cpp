#include "macro_bind.h"
#include "scope_chain.h"
#include "invocation_frame.h"
#include "structural_detail.h"

#include "meios/xacro/eval_scope.h"
#include "meios/xacro/structural.h"

#include "meios/diagnostic/diagnostic_code.h"

#include <pugixml.hpp>

#include <map>
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

// The reference clones the definition element and evaluates the clone before discarding the
// wrapper, so every parameter default is evaluated once per instantiation, in the invocation's
// own scope, whether or not the caller supplied that parameter, and the result is thrown away.
// A default naming an unbound property therefore refuses there even where the call supplied the
// parameter -- measured against xacro 2.1.1, which renders `^` and `^|text` unchanged in the same
// position, so what runs here is the substitution of the authored text and not a second reading
// of the inheritance arms. It runs before the definition's origin is pushed, so a refusal is
// located at the call.
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

}

bool define_macro(expand_ctx &ctx, pugi::xml_node in)
{
    macro_def def;
    const std::string name = in.attribute("name").value();
    // The reference refuses such a list when it reads the definition, called or not.
    if(!parse_params(in.attribute("params").value(), def))
        return fail(ctx, in, diagnostic_code::xacro_structural_error, "macro '" + name + "' has a params attribute that holds only whitespace");
    def.body = in;
    if(!ctx.origins.empty())
        def.origin = ctx.origins.back();
    ctx.macros.define(name, std::move(def));
    return true;
}

bool instantiate_macro(expand_ctx &ctx, const macro_def &def, pugi::xml_node call,
                       pugi::xml_node out, const std::filesystem::path &document)
{
    call_binding bound;
    if(!ctx.charge_work(call) || !bind_call(ctx, def, call, document, bound))
        return false;
    const invocation_frame frame(ctx.scope, ctx.macros);
    std::map<std::string, block_arg> outer = std::exchange(ctx.blocks, std::move(bound.blocks));
    for(std::size_t i = 0; i < def.params.size(); ++i)
        chain_access::define(ctx.scope, frame.table(), def.params[i], bound.values[i]);
    bool ok = substitute_defaults(ctx, def, call, document);
    if(ok)
    {
        ctx.origins.push_back(def.origin);
        ok = process_children(ctx, def.body, out, document);
        ctx.origins.pop_back();
    }
    ctx.blocks = std::move(outer);
    return ok;
}

}
