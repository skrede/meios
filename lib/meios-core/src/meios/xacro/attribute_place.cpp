#include "dispatch_view.h"
#include "structural_detail.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/source_location.h"

#include <pugixml.hpp>

#include <string>
#include <cstddef>
#include <filesystem>

namespace meios::detail
{

namespace
{

void overwrite(pugi::xml_node element, const std::string &name, const std::string &value)
{
    pugi::xml_attribute held = element.attribute(name.c_str());
    if(!held)
        held = element.append_attribute(name.c_str());
    held.set_value(value.c_str());
}

void note(expand_ctx &ctx, const source_location &where, const std::string &name, pugi::xml_node onto)
{
    ctx.log.log(level::info, where, "attribute '" + name + "' placed on <" + onto.name() + ">, the element its parent's content lands in");
}

void note_dropped(expand_ctx &ctx, const placed_attribute &one)
{
    ctx.log.log(level::info, one.where, "attribute '" + one.name + "' is not set: the block holding it was passed whole into another call, which inserts only its content");
}

bool receives(const expand_ctx &ctx, pugi::xml_node out)
{
    return !ctx.emitting.empty() && !ctx.emitting.back().binding && ctx.emitting.back().out == out;
}

bool parked_argument(const expand_ctx &ctx)
{
    return ctx.emitting.size() > 1 && ctx.emitting[ctx.emitting.size() - 2].binding;
}

void place(expand_ctx &ctx, pugi::xml_node out, const std::string &name, const std::string &value, const source_location &where, bool moved)
{
    if(!receives(ctx, out))
    {
        ctx.log.log(level::warn, where, "attribute '" + name + "' is written where no element receives it, so it is not set");
        return;
    }
    overwrite(out, name, value);
    if(parked_argument(ctx))
        ctx.placed.push_back(placed_attribute{out, name, where, moved, false});
    else if(moved)
        note(ctx, where, name, out);
}

}

emitting_scope::emitting_scope(expand_ctx &ctx, emission current)
    : m_ctx(ctx)
{
    m_ctx.emitting.push_back(current);
}

emitting_scope::~emitting_scope()
{
    m_ctx.emitting.pop_back();
}

// The parent a construct discards is never the element being emitted, so an attribute it held is
// placed on the element its content lands in, and the note says so.
bool set_attribute(expand_ctx &ctx, const dispatch_view &view, pugi::xml_node out, const std::filesystem::path &document)
{
    bool ok = true;
    const std::string name  = evaluated_name(ctx, view, "name", document, ok);
    const std::string value = ok ? evaluated_text(ctx, view, "value", document, ok) : std::string();
    if(!ok)
        return false;
    const bool moved = ctx.emitting.empty() || ctx.emitting.back().in != view.node.parent();
    place(ctx, out, name, value, locate(ctx, view.node), moved);
    return true;
}

// Placing may record against the element out, so the records are read by index up to the count
// held on entry.
void carry_placed(expand_ctx &ctx, pugi::xml_node parked, pugi::xml_node out)
{
    const std::size_t held = ctx.placed.size();
    for(std::size_t i = 0; i < held; ++i)
    {
        const placed_attribute one = ctx.placed[i];
        if(one.element == parked && one.forwarded)
            note_dropped(ctx, one);
        else if(one.element == parked)
            place(ctx, out, one.name, parked.attribute(one.name.c_str()).value(), one.where, true);
    }
}

void note_placed(expand_ctx &ctx, pugi::xml_node parked)
{
    for(const placed_attribute &one : ctx.placed)
        if(one.element == parked && one.moved)
            note(ctx, one.where, one.name, parked);
}

}
