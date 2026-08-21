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

// A `^` default inherits the enclosing value; `^|fallback` inherits it or the
// fallback text; a bare `^` with no inherited value is a loud error, never literal.
bool bind_default(expand_ctx &ctx, pugi::xml_node call, const std::string &name,
                  const std::string &def, const std::filesystem::path &document, value &bound)
{
    if(def != "^" && def.rfind("^|", 0) != 0)
        return bind_literal(ctx, call, def, document, bound);
    if(std::optional<value> inherited = ctx.scope.lookup(name))
    {
        bound = *inherited;
        return true;
    }
    if(def.rfind("^|", 0) == 0)
        return bind_literal(ctx, call, std::string_view(def).substr(2), document, bound);
    return fail(ctx, call, diagnostic_code::xacro_structural_error,
                "macro parameter '" + name + "' inherits no value and has no fallback");
}

bool bind_one(expand_ctx &ctx, const macro_def &def, pugi::xml_node call, std::size_t at,
              const std::filesystem::path &document, value &bound)
{
    const std::string &name = def.params[at];
    pugi::xml_attribute attr = call.attribute(name.c_str());
    if(!attr && !def.defaults[at])
        return fail(ctx, call, diagnostic_code::xacro_structural_error,
                    "macro instantiation is missing required parameter '" + name + '\'');
    return attr ? bind_literal(ctx, call, attr.value(), document, bound)
                : bind_default(ctx, call, name, *def.defaults[at], document, bound);
}

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

// Every argument is evaluated before any of them is bound, because the reference evaluates a
// call's arguments in the caller's own symbol table and writes them into a table the body
// alone reads. Binding as it went would let one argument read a sibling's new value -- which
// a description does write, passing both `ee_id="${ee_id}_${ee_color}"` and a second argument
// naming `ee_id`, where the two spellings mean different things.
bool bind_params(expand_ctx &ctx, const macro_def &def, pugi::xml_node call,
                 const std::filesystem::path &document, std::vector<saved_binding> &saved)
{
    std::vector<value> bound(def.params.size());
    for(std::size_t i = 0; i < def.params.size(); ++i)
        if(!bind_one(ctx, def, call, i, document, bound[i]))
            return false;
    for(std::size_t i = 0; i < def.params.size(); ++i)
    {
        saved.emplace_back(def.params[i], ctx.scope.lookup(def.params[i]));
        ctx.scope.set(def.params[i], bound[i]);
    }
    return true;
}

// The reference evaluates the whole call node in the caller's own macros and symbols before it
// captures a single block, so a block argument means what it means where the caller wrote it and
// is evaluated whether or not the receiving macro ever inserts it. That is also what terminates a
// forwarded insert: the callee binds a literal subtree rather than an insert command that would
// resolve again against the binding which produced it. The order below is the contract -- expand
// while the caller's blocks are still installed, then move them aside, then bind.
bool bind_blocks(expand_ctx &ctx, const macro_def &def, pugi::xml_node call,
                 const std::filesystem::path &document, std::map<std::string, block_arg> &outer)
{
    pugi::xml_node root = ctx.park().append_child("blocks");
    if(!process_children(ctx, call, root, document))
        return false;
    std::vector<pugi::xml_node> kids;
    for(pugi::xml_node child : root.children())
        if(child.type() == pugi::node_element)
            kids.push_back(child);
    outer = std::move(ctx.blocks);
    ctx.blocks.clear();
    for(std::size_t i = 0; i < def.block_params.size() && i < kids.size(); ++i)
        ctx.blocks.insert_or_assign(def.block_params[i],
                                    block_arg{ def.block_children[i], kids[i] });
    return true;
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
    std::map<std::string, block_arg> outer_blocks;
    if(!bind_blocks(ctx, def, call, document, outer_blocks))
        return false;
    std::vector<saved_binding> saved;
    ctx.prop_frames.emplace_back();
    ctx.param_saves.push_back(&saved);
    ctx.origins.push_back(def.origin);
    bool ok = bind_params(ctx, def, call, document, saved)
           && substitute_defaults(ctx, def, call, document)
           && process_children(ctx, def.body, out, document);
    ctx.origins.pop_back();
    ctx.param_saves.pop_back();
    revert_prop_frame(ctx);
    restore_params(ctx, saved);
    ctx.blocks = std::move(outer_blocks);
    return ok;
}

}
