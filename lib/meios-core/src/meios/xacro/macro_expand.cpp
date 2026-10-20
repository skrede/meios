#include "macro_bind.h"
#include "structural_detail.h"

#include "meios/xacro/eval_scope.h"
#include "meios/xacro/structural.h"

#include "meios/diagnostic/diagnostic_code.h"

#include <pugixml.hpp>

#include <map>
#include <ranges>
#include <string>
#include <vector>
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
// of the inheritance arms.
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

void set_params(expand_ctx &ctx, const macro_def &def, const std::vector<value> &values,
                std::vector<saved_binding> &saved)
{
    for(std::size_t i = 0; i < def.params.size(); ++i)
    {
        saved.emplace_back(def.params[i], ctx.scope.lookup(def.params[i]));
        ctx.scope.set(def.params[i], values[i]);
    }
}

void restore_params(expand_ctx &ctx, const std::vector<saved_binding> &saved)
{
    for(const saved_binding &entry : saved)
    {
        if(entry.second)
            ctx.scope.set(entry.first, *entry.second);
        else
            ctx.scope.erase(entry.first);
    }
}

// Reverting in reverse restores the true pre-invocation value when a name was written
// more than once inside the frame.
void revert_prop_frame(expand_ctx &ctx)
{
    prop_frame &frame = ctx.prop_frames.back();
    for(const saved_binding &entry : std::views::reverse(frame))
    {
        if(entry.second)
            ctx.scope.set(entry.first, *entry.second);
        else
            ctx.scope.erase(entry.first);
    }
    ctx.prop_frames.pop_back();
}

}

void define_macro(expand_ctx &ctx, pugi::xml_node in)
{
    macro_def def;
    parse_params(in.attribute("params").value(), def);
    def.body = in;
    if(!ctx.origins.empty())
        def.origin = ctx.origins.back();
    ctx.macros[in.attribute("name").value()] = std::move(def);
}

bool instantiate_macro(expand_ctx &ctx, const macro_def &def, pugi::xml_node call,
                       pugi::xml_node out, const std::filesystem::path &document)
{
    if(!ctx.charge_work(call))
        return false;
    call_binding bound;
    if(!bind_call(ctx, def, call, document, bound))
        return false;
    std::vector<saved_binding> saved;
    ctx.prop_frames.emplace_back();
    ctx.param_saves.push_back(&saved);
    ctx.origins.push_back(def.origin);
    std::map<std::string, block_arg> outer = std::exchange(ctx.blocks, std::move(bound.blocks));
    set_params(ctx, def, bound.values, saved);
    bool ok = substitute_defaults(ctx, def, call, document)
           && process_children(ctx, def.body, out, document);
    ctx.origins.pop_back();
    ctx.param_saves.pop_back();
    revert_prop_frame(ctx);
    restore_params(ctx, saved);
    ctx.blocks = std::move(outer);
    return ok;
}

}
