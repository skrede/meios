#include "structural_detail.h"

#include "meios/xacro/eval_scope.h"

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

void parse_param(std::string_view token, macro_def &def)
{
    if(token.rfind("**", 0) == 0)
    {
        def.block_params.emplace_back(token.substr(2));
        def.block_children.push_back(true);
    }
    else if(token.front() == '*')
    {
        def.block_params.emplace_back(token.substr(1));
        def.block_children.push_back(false);
    }
    else if(std::size_t colon = token.find(":="); colon != std::string_view::npos)
    {
        def.params.emplace_back(token.substr(0, colon));
        def.defaults.emplace_back(std::string(token.substr(colon + 2)));
    }
    else
    {
        def.params.emplace_back(token);
        def.defaults.emplace_back(std::nullopt);
    }
}

void parse_params(std::string_view spec, macro_def &def)
{
    for(std::size_t i = 0; i < spec.size();)
    {
        std::size_t space = spec.find_first_of(" \t\n\r", i);
        std::string_view token =
            spec.substr(i, space == std::string_view::npos ? space : space - i);
        if(!token.empty())
            parse_param(token, def);
        if(space == std::string_view::npos)
            break;
        i = space + 1;
    }
}

bool bind_literal(expand_ctx &ctx, pugi::xml_node call, const std::string &name,
                  std::string_view raw, const std::filesystem::path &document)
{
    bool ok = true;
    std::string bound = substitute_attr(ctx, call, raw, document, ok);
    if(!ok)
        return false;
    ctx.scope.set(name, classify(bound));
    return true;
}

// A `^` default inherits the enclosing binding; `^|fallback` inherits it or the
// fallback text; a bare `^` with no inherited value is a loud error, never literal.
bool bind_default(expand_ctx &ctx, pugi::xml_node call, const std::string &name,
                  const std::string &def, const std::filesystem::path &document)
{
    if(def != "^" && def.rfind("^|", 0) != 0)
        return bind_literal(ctx, call, name, def, document);
    if(std::optional<binding> inherited = ctx.scope.lookup(name))
    {
        ctx.scope.set(name, *inherited);
        return true;
    }
    if(def.rfind("^|", 0) == 0)
        return bind_literal(ctx, call, name, std::string_view(def).substr(2), document);
    return fail(ctx, call, diagnostic_code::xacro_structural_error,
                "macro parameter '" + name + "' inherits no value and has no fallback");
}

bool bind_params(expand_ctx &ctx, const macro_def &def, pugi::xml_node call,
                 const std::filesystem::path &document, std::vector<saved_binding> &saved)
{
    for(std::size_t i = 0; i < def.params.size(); ++i)
    {
        const std::string &name = def.params[i];
        saved.emplace_back(name, ctx.scope.lookup(name));
        pugi::xml_attribute attr = call.attribute(name.c_str());
        if(!attr && !def.defaults[i])
            return fail(ctx, call, diagnostic_code::xacro_structural_error,
                        "macro instantiation is missing required parameter '" + name + '\'');
        const bool ok = attr ? bind_literal(ctx, call, name, attr.value(), document)
                             : bind_default(ctx, call, name, *def.defaults[i], document);
        if(!ok)
            return false;
    }
    return true;
}

void bind_blocks(const macro_def &def, pugi::xml_node call, std::map<std::string, block_arg> &blocks,
                 const emit_origin &origin)
{
    std::vector<pugi::xml_node> kids;
    for(pugi::xml_node child : call.children())
        if(child.type() == pugi::node_element)
            kids.push_back(child);
    for(std::size_t i = 0; i < def.block_params.size() && i < kids.size(); ++i)
        blocks.insert_or_assign(def.block_params[i],
                                block_arg{ def.block_children[i], kids[i], origin });
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
    std::vector<saved_binding> saved;
    std::map<std::string, block_arg> outer_blocks = std::move(ctx.blocks);
    ctx.blocks.clear();
    bind_blocks(def, call, ctx.blocks, ctx.origins.empty() ? emit_origin{} : ctx.origins.back());
    ctx.prop_frames.emplace_back();
    ctx.param_saves.push_back(&saved);
    ctx.origins.push_back(def.origin);
    bool ok = bind_params(ctx, def, call, document, saved)
           && process_children(ctx, def.body, out, document);
    ctx.origins.pop_back();
    ctx.param_saves.pop_back();
    revert_prop_frame(ctx);
    restore_params(ctx, saved);
    ctx.blocks = std::move(outer_blocks);
    return ok;
}

}
