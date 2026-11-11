#include "scope_chain.h"
#include "literal_read.h"
#include "block_entries.h"
#include "boolean_value.h"
#include "deferred_read.h"
#include "structural_detail.h"

#include "meios/xacro/value.h"
#include "meios/xacro/structural.h"
#include "meios/xacro/eval_scope.h"

#include "meios/diagnostic/diagnostic_code.h"

#include <pugixml.hpp>

#include <array>
#include <string>
#include <cstddef>
#include <utility>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

namespace
{

// remove and lazy_eval are read from what evaluating their text returns, so an attribute that is
// one exact expression is read by its type and any other by its text; an empty one reads as the
// attribute's own fallback.
std::optional<bool> flag_attribute(expand_ctx &ctx, pugi::xml_node in, const char *attribute, bool fallback,
                                   const std::filesystem::path &document)
{
    const std::string authored = strip_authored_markers(in.attribute(attribute).value());
    if(authored.empty())
        return fallback;
    bool ok = true;
    const value read = substitute_arg_value(ctx, in, authored, document, ok, attr_dom_index(in, attribute));
    if(!ok)
        return std::nullopt;
    const std::optional<bool> flag = get_boolean_value(read);
    if(!flag)
        fail(ctx, in, diagnostic_code::xacro_structural_error,
             "xacro:property " + std::string(attribute) + "=\"" + authored + "\" evaluates to \"" + read.text().value_or("") + "\", which is not a boolean");
    return flag;
}

bool refuse_parent(expand_ctx &ctx, pugi::xml_node in, std::string_view name)
{
    return fail(ctx, in, diagnostic_code::xacro_structural_error,
                "xacro:property \"" + std::string(name)
                    + "\" is stored in the parent scope, and at document level there is no parent table to store it in");
}

bool define_eager(expand_ctx &ctx, pugi::xml_node in, const char *attribute,
                  const std::filesystem::path &document, std::string_view name)
{
    bool ok = true;
    const std::string authored = strip_authored_markers(in.attribute(attribute).value());
    const value bound = substitute_attr_value(ctx, in, authored, document, ok, attr_dom_index(in, attribute));
    if(!ok)
        return false;
    const std::optional<std::size_t> target = property_target(ctx, in.attribute("scope").value());
    if(!target)
        return refuse_parent(ctx, in, name);
    warn_global_symbol(ctx, in, name);
    chain_access::define(ctx.scope, *target, name, bound);
    return true;
}

// The property element itself is stored under the block key `**name`, which no read and no
// built-in shares, and its children are inserted wherever insert_block names it.
bool define_block_property(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document,
                           std::string_view name)
{
    const std::optional<std::size_t> target = property_target(ctx, in.attribute("scope").value());
    if(!target)
        return refuse_parent(ctx, in, name);
    const emit_origin origin = ctx.origins.empty() ? emit_origin{ document, {} } : ctx.origins.back();
    ctx.blocks.push_back(parked_block{ in, true, origin });
    chain_access::define_block(ctx.scope, *target, "**" + std::string(name), ctx.blocks.size() - 1);
    return true;
}

// A property stored in another table than the current one is evaluated where it is written,
// whatever lazy_eval says.
bool deferrable(pugi::xml_node in)
{
    const std::string_view scope = in.attribute("scope").value();
    return scope != "global" && scope != "parent";
}

// A lazy text is read as a literal, and only what stays a string is deferred, with its enclosing
// quotes already removed. A string with no '$' is read a second time instead of being deferred,
// since evaluating it gives the same text and a resolved value is read as a literal too.
void define_lazy(expand_ctx &ctx, pugi::xml_node in, const char *attribute,
                 const std::filesystem::path &document, std::string_view name)
{
    warn_global_symbol(ctx, in, name);
    const std::size_t table = chain_access::cursor(ctx.scope);
    value literal = read_literal(strip_authored_markers(in.attribute(attribute).value()));
    std::optional<std::string> text = literal.text();
    if(text && text->find('$') != std::string::npos)
    {
        const std::size_t origin = record_origin(ctx, in, attribute, document, name);
        chain_access::define_deferred(ctx.scope, table, name, std::move(*text), origin);
        return;
    }
    chain_access::define(ctx.scope, table, name, text ? read_literal(*text) : std::move(literal));
}

bool store(expand_ctx &ctx, pugi::xml_node in, const char *attribute, const std::filesystem::path &document,
           std::string_view name)
{
    const std::optional<bool> lazy = flag_attribute(ctx, in, "lazy_eval", true, document);
    if(!lazy)
        return false;
    if(!in.attribute(attribute))
        return define_block_property(ctx, in, document, name);
    if(!*lazy || !deferrable(in))
        return define_eager(ctx, in, attribute, document, name);
    define_lazy(ctx, in, attribute, document, name);
    return true;
}

}

// In this order: the name is evaluated and checked first, remove is read before the three
// attributes are held exclusive, a removal acts only on a defined name, and a fallback binds only
// where the name is not defined, a built-in counting as defined and an argument of the same name not
// counting. A property carrying neither value nor default is a block, a removal of a name no table
// holds included.
bool define_property(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document)
{
    const std::optional<std::string> name = property_name(ctx, in, document);
    if(!name)
        return false;
    const std::optional<bool> remove = flag_attribute(ctx, in, "remove", false, document);
    if(!remove)
        return false;
    const bool fallback = static_cast<bool>(in.attribute("default"));
    const std::array<bool, 3> given{ static_cast<bool>(in.attribute("value")), fallback, *remove };
    if(std::ranges::count(given, true) > 1)
        return fail(ctx, in, diagnostic_code::xacro_structural_error, "xacro:property '" + *name + "' sets more than one of default, value and remove");
    if(*remove && remove_property(ctx, in, *name))
        return true;
    if(fallback && property_defined(ctx, *name))
        return true;
    return store(ctx, in, fallback ? "default" : "value", document, *name);
}

}
