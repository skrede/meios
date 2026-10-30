#include "scope_chain.h"
#include "block_entries.h"
#include "structural_detail.h"

#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"

#include <pugixml.hpp>

#include <string>
#include <cstddef>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

namespace
{

// The reference evaluates a cloned block's own attributes before its children even where only the
// children are inserted, so a wrapper attribute that no longer evaluates refuses the insertion.
bool evaluate_wrapper(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document)
{
    std::size_t attr_index = 0;
    for(pugi::xml_attribute attr : in.attributes())
    {
        bool ok = true;
        if(std::string_view(attr.name()) != "xmlns:xacro")
            substitute_attr(ctx, in, attr.value(), document, ok, attr_index);
        if(!ok)
            return false;
        ++attr_index;
    }
    return true;
}

bool walk_block(expand_ctx &ctx, const parked_block &block, pugi::xml_node out,
                const std::filesystem::path &document)
{
    if(!block.content_only)
        return emit_element(ctx, block.node, out, document);
    return evaluate_wrapper(ctx, block.node, document) && process_children(ctx, block.node, out, document);
}

}

// The reference clones the block and evaluates the clone with the inserting tables, so the text
// the call's walk left, an escaped span among it, is evaluated again at each insertion. The record
// is copied because a call the walk reaches may bind blocks of its own and grow the list.
bool insert_block(expand_ctx &ctx, pugi::xml_node in, pugi::xml_node out,
                  const std::filesystem::path &document)
{
    const std::string name = in.attribute("name").value();
    const std::optional<std::size_t> found = chain_access::find_block(ctx.scope, name);
    if(!found)
        return fail(ctx, in, diagnostic_code::xacro_structural_error, "Undefined block \"" + name + '"');
    const parked_block block = ctx.blocks[*found];
    std::optional<source_location> outer = std::exchange(ctx.insert_site, locate(ctx, in));
    ctx.origins.push_back(block.origin);
    const bool ok = walk_block(ctx, block, out, document);
    ctx.origins.pop_back();
    ctx.insert_site = std::move(outer);
    return ok;
}

}
