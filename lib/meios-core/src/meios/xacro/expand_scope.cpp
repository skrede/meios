#include "scope_chain.h"
#include "boolean_value.h"
#include "structural_detail.h"

#include "meios/xacro/value.h"
#include "meios/xacro/structural.h"
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
        return chain_access::parent(ctx.scope, current).value_or(chain_access::document_table);
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

std::optional<value> authored_value(expand_ctx &ctx, pugi::xml_node in, const char *attribute,
                                    const std::filesystem::path &document)
{
    bool ok = true;
    const std::string authored = strip_authored_markers(in.attribute(attribute).value());
    value bound = substitute_attr_value(ctx, in, authored, document, ok, attr_dom_index(in, attribute));
    if(!ok)
        return std::nullopt;
    return bound;
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
    const std::optional<value> bound = authored_value(ctx, in, fallback ? "default" : "value", document);
    if(!bound)
        return false;
    chain_access::define(ctx.scope, target_table(ctx, in.attribute("scope").value()), name, *bound);
    return true;
}

}
