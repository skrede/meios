#include "scope_chain.h"
#include "boolean_value.h"
#include "deferred_read.h"
#include "structural_detail.h"

#include "meios/xacro/value.h"
#include "meios/xacro/structural.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/value_kind.h"
#include "meios/xacro/detail/numeric.h"

#include "meios/diagnostic/diagnostic_code.h"

#include <pugixml.hpp>

#include <string>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

// The number kinds are tried before the boolean spellings, which is what leaves "1" an
// integer rather than a true; the reference orders its own literal conversion the same way,
// and only these four spellings ever reach the boolean arm. A description passing
// no_prefix="false" into a macro and testing it for truth means the opposite of what a
// non-empty string would mean, so the conversion is what keeps the two readings the same.
value classify(std::string_view text)
{
    bool ok = false;
    const std::int64_t integer = parse_int(text, ok);
    if(ok)
        return value{ integer };
    const double real = parse_double(text, ok);
    if(ok)
        if(std::optional<value> number = value::make_real(real))
            return *number;
    if(text == "true" || text == "True")
        return value{ true };
    if(text == "false" || text == "False")
        return value{ false };
    return value{ std::string(text) };
}

namespace
{

// The reference skips namespaces only when the current table is not one, which is a macro call's,
// so a macro a namespace holds writes into its caller's table, while a namespaced file's own
// top level writes into the including table.
std::size_t parent_table(const eval_scope &scope, std::size_t current)
{
    std::optional<std::size_t> target = chain_access::parent(scope, current);
    if(!chain_access::is_namespace(scope, current))
        while(target && chain_access::is_namespace(scope, *target))
            target = chain_access::parent(scope, *target);
    return target.value_or(chain_access::document_table);
}

// The reference picks the table a property is stored in by the scope attribute alone: the
// document's for "global", the caller's for "parent", and the current one for any other value, an
// unknown one included. A document-level "parent", which the reference stores into its table of
// built-ins, lands in the document table.
std::size_t target_table(const expand_ctx &ctx, std::string_view scope_attr)
{
    const std::size_t current = chain_access::cursor(ctx.scope);
    if(scope_attr == "global")
        return chain_access::document_table;
    if(scope_attr == "parent")
        return parent_table(ctx.scope, current);
    return current;
}

// The reference reads lazy_eval from what its text evaluation returns, so an attribute that is one
// exact expression is read by its type and any other by its text; an empty one reads true.
std::optional<bool> lazy_eval(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document)
{
    const std::string authored = strip_authored_markers(in.attribute("lazy_eval").value());
    if(authored.empty())
        return true;
    bool ok = true;
    const value read = substitute_arg_value(ctx, in, authored, document, ok, attr_dom_index(in, "lazy_eval"));
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
    chain_access::define(ctx.scope, target_table(ctx, in.attribute("scope").value()), name, bound);
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

}

// The reference's grab_property treats the written value and the fallback as mutually exclusive
// and binds a fallback only where no property of the name exists; an argument of the same name
// does not suppress it. A property carrying neither attribute binds the empty string.
bool define_property(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document)
{
    pugi::xml_attribute written = in.attribute("value");
    pugi::xml_attribute fallback = in.attribute("default");
    if(written && fallback)
        return fail(ctx, in, diagnostic_code::xacro_structural_error,
                    "<xacro:property> writes a value attribute and a default attribute together, "
                    "which are mutually exclusive");
    std::string_view name = in.attribute("name").value();
    if(fallback && ctx.scope.contains(name))
        return true;
    const std::optional<bool> lazy = lazy_eval(ctx, in, document);
    if(!lazy)
        return false;
    const char *attribute = fallback ? "default" : "value";
    if(!*lazy || !deferrable(in))
        return define_eager(ctx, in, attribute, document, name);
    define_lazy(ctx, in, attribute, document, name);
    return true;
}

}
