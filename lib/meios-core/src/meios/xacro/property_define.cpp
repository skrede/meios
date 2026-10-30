#include "scope_chain.h"
#include "boolean_value.h"
#include "deferred_read.h"
#include "structural_detail.h"

#include "meios/xacro/value.h"
#include "meios/xacro/structural.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/value_kind.h"

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

// The reference reads remove and lazy_eval from what its text evaluation returns, so an attribute
// that is one exact expression is read by its type and any other by its text; an empty one reads
// as the attribute's own fallback.
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
             "Xacro conditional \"" + authored + "\" evaluated to \"" + read.text().value_or("")
                 + "\", which is not a boolean expression.");
    return flag;
}

bool define_eager(expand_ctx &ctx, pugi::xml_node in, const char *attribute,
                  const std::filesystem::path &document, std::string_view name)
{
    bool ok = true;
    const std::string authored = strip_authored_markers(in.attribute(attribute).value());
    const value bound = substitute_attr_value(ctx, in, authored, document, ok, attr_dom_index(in, attribute));
    if(!ok)
        return false;
    chain_access::define(ctx.scope, property_target(ctx, in.attribute("scope").value()), name, bound);
    return true;
}

// The reference evaluates a property it stores in another table than the current one where it is
// written, whatever lazy_eval says.
bool deferrable(pugi::xml_node in)
{
    const std::string_view scope = in.attribute("scope").value();
    return scope != "global" && scope != "parent";
}

// The reference converts a lazy text to a number or a boolean when it reads as one and defers only
// what stays a string. A string with no '$' is stored as written, which is what evaluating it
// would give.
void define_lazy(expand_ctx &ctx, pugi::xml_node in, const char *attribute,
                 const std::filesystem::path &document, std::string_view name)
{
    const std::size_t table = chain_access::cursor(ctx.scope);
    std::string authored = strip_authored_markers(in.attribute(attribute).value());
    value literal = classify(authored);
    if(literal.kind() != value_kind::string || authored.find('$') == std::string::npos)
    {
        chain_access::define(ctx.scope, table, name, std::move(literal));
        return;
    }
    const std::size_t origin = record_origin(ctx, in, attribute, document);
    chain_access::define_deferred(ctx.scope, table, name, std::move(authored), origin);
}

bool store(expand_ctx &ctx, pugi::xml_node in, const char *attribute, const std::filesystem::path &document,
           std::string_view name)
{
    const std::optional<bool> lazy = flag_attribute(ctx, in, "lazy_eval", true, document);
    if(!lazy)
        return false;
    if(!*lazy || !deferrable(in))
        return define_eager(ctx, in, attribute, document, name);
    define_lazy(ctx, in, attribute, document, name);
    return true;
}

}

// The reference's grab_property, in its order: remove is read before the three attributes are
// held exclusive, a removal acts only on a name some table holds, and a fallback binds only where
// none does; an argument of the same name does not count. A property carrying neither value nor
// default binds the empty string, a removal of a name no table holds included.
bool define_property(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document)
{
    const std::optional<bool> remove = flag_attribute(ctx, in, "remove", false, document);
    if(!remove)
        return false;
    const std::string_view name = in.attribute("name").value();
    const bool fallback = static_cast<bool>(in.attribute("default"));
    const std::array<bool, 3> given{ static_cast<bool>(in.attribute("value")), fallback, *remove };
    if(std::ranges::count(given, true) > 1)
        return fail(ctx, in, diagnostic_code::xacro_structural_error,
                    "Property attributes default, value, and remove are mutually exclusive: " + std::string(name));
    if(*remove && remove_property(ctx, name))
        return true;
    if(fallback && ctx.scope.contains(name))
        return true;
    return store(ctx, in, fallback ? "default" : "value", document, name);
}

}
