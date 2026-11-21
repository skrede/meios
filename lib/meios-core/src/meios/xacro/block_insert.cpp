#include "scope_chain.h"
#include "block_entries.h"
#include "namespace_lift.h"
#include "structural_detail.h"

#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"

#include <pugixml.hpp>

#include <string>
#include <cstddef>
#include <utility>
#include <optional>
#include <filesystem>

namespace meios::detail
{

namespace
{

// A cloned block's own attributes are evaluated before its children even where only the children
// are inserted, so a wrapper attribute that no longer evaluates refuses the insertion.
bool evaluate_wrapper(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document)
{
    std::size_t attr_index = 0;
    for(pugi::xml_attribute attr : in.attributes())
    {
        bool ok = true;
        if(!xacro_namespace_attribute(attr.name()))
            substitute_attr(ctx, in, attr.value(), document, ok, attr_index);
        if(!ok)
            return false;
        ++attr_index;
    }
    return true;
}

// A block passed whole into another call's arguments is a copy, so its records travel with it.
void forward_placed(expand_ctx &ctx, pugi::xml_node parked, pugi::xml_node copy)
{
    const std::size_t held = ctx.placed.size();
    for(std::size_t i = 0; i < held; ++i)
    {
        placed_attribute one = ctx.placed[i];
        if(one.element != parked)
            continue;
        one.element   = copy;
        one.forwarded = true;
        ctx.placed.push_back(std::move(one));
    }
}

bool walk_block(expand_ctx &ctx, const parked_block &block, pugi::xml_node out,
                const std::filesystem::path &document)
{
    if(!block.content_only)
    {
        if(!emit_element(ctx, block.node, block.node.name(), out, document))
            return false;
        // emit_element appends that one element to out.
        if(!ctx.emitting.empty() && ctx.emitting.back().binding)
            forward_placed(ctx, block.node, out.last_child());
        else
            note_placed(ctx, block.node);
        return true;
    }
    const lift_target dropped(ctx, {});
    if(!evaluate_wrapper(ctx, block.node, document))
        return false;
    carry_placed(ctx, block.node, out);
    return process_children(ctx, block.node, out, document);
}

}

// A cloned block is evaluated with the inserting tables, so the text the call's walk left, an
// escaped span among it, is evaluated again at each insertion. The record is copied because a call
// the walk reaches may bind blocks of its own and grow the list.
bool insert_block(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                  const std::filesystem::path &document)
{
    const std::string name = in.attribute("name").value();
    const std::optional<std::size_t> found = chain_access::find_block(ctx.scope, name);
    if(!found)
        return fail(ctx, in, diagnostic_code::xacro_structural_error, "<xacro:insert_block> names '" + name + "', which no block parameter in scope binds");
    const parked_block block = ctx.blocks[*found];
    std::optional<source_location> outer = std::exchange(ctx.insert_site, locate(ctx, in));
    ctx.origins.push_back(block.origin);
    const bool ok = walk_block(ctx, block, out, document);
    ctx.origins.pop_back();
    ctx.insert_site = std::move(outer);
    return ok;
}

}
