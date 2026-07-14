#include "structural_detail.h"

#include "meios/xacro/eval_scope.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <pugixml.hpp>

#include <map>
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

using saved_binding = std::pair<std::string, std::optional<binding>>;

bool bind_params(expand_ctx &ctx, const macro_def &def, pugi::xml_node call,
                 const std::filesystem::path &document, std::vector<saved_binding> &saved)
{
    for(std::size_t i = 0; i < def.params.size(); ++i)
    {
        const std::string &name = def.params[i];
        saved.emplace_back(name, ctx.scope.lookup(name));
        pugi::xml_attribute attr = call.attribute(name.c_str());
        if(!attr && !def.defaults[i])
            return fail(ctx, "macro instantiation is missing required parameter '" + name + '\'');
        bool ok = true;
        std::string raw = attr ? std::string(attr.value()) : *def.defaults[i];
        std::string bound = substitute_attr(ctx, raw, document, ok);
        if(!ok)
            return false;
        ctx.scope.set(name, classify(bound));
    }
    return true;
}

void bind_blocks(const macro_def &def, pugi::xml_node call, std::map<std::string, block_arg> &blocks)
{
    std::vector<pugi::xml_node> kids;
    for(pugi::xml_node child : call.children())
        if(child.type() == pugi::node_element)
            kids.push_back(child);
    for(std::size_t i = 0; i < def.block_params.size() && i < kids.size(); ++i)
        blocks[def.block_params[i]] = block_arg{ def.block_children[i], kids[i] };
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

}

void define_macro(expand_ctx &ctx, pugi::xml_node in)
{
    macro_def def;
    parse_params(in.attribute("params").value(), def);
    def.body = in;
    ctx.macros[in.attribute("name").value()] = std::move(def);
}

bool instantiate_macro(expand_ctx &ctx, const macro_def &def, pugi::xml_node call,
                       pugi::xml_node out, const std::filesystem::path &document)
{
    if(!ctx.charge_work())
        return false;
    std::vector<saved_binding> saved;
    std::map<std::string, block_arg> outer_blocks = std::move(ctx.blocks);
    ctx.blocks.clear();
    bind_blocks(def, call, ctx.blocks);
    bool ok = bind_params(ctx, def, call, document, saved)
           && process_children(ctx, def.body, out, document);
    restore_params(ctx, saved);
    ctx.blocks = std::move(outer_blocks);
    return ok;
}

}
