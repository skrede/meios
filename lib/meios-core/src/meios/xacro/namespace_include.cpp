#include "scope_chain.h"
#include "structural_detail.h"

#include "meios/xacro/value.h"
#include "meios/xacro/structural.h"
#include "meios/xacro/value_render.h"

#include "meios/diagnostic/diagnostic_code.h"

#include <pugixml.hpp>

#include <string>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

namespace
{

// The reference keys both tables by whatever the evaluation returns and refuses a collection,
// which cannot key them; a scalar keys them by its rendered text here.
std::optional<std::string> namespace_name(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document, bool &ok)
{
    const std::string authored = strip_authored_markers(in.attribute("ns").value());
    const value evaluated = substitute_arg_value(ctx, in, authored, document, ok, attr_dom_index(in, "ns"));
    if(!ok)
        return std::nullopt;
    std::optional<std::string> name = render_scalar(evaluated);
    if(!name)
        ok = fail(ctx, in, diagnostic_code::xacro_structural_error,
                  "xacro:include ns=\"" + authored + "\" evaluates to a " + std::string(kind_name(evaluated.kind()))
                      + ", and a namespace must be named by text");
    return name;
}

}

// The reference tests the attribute as written, so an empty one reads the file into the including
// tables, and it evaluates a namespace in the including table before it reads the filename.
include_scope include_table(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document, bool &ok)
{
    const include_scope including{ chain_access::cursor(ctx.scope), ctx.macros.cursor() };
    ok = true;
    if(std::string_view(in.attribute("ns").value()).empty())
        return including;
    const std::optional<std::string> name = namespace_name(ctx, in, document, ok);
    if(!ok)
        return including;
    const std::size_t macros = ctx.macros.open_namespace(*name);
    warn_global_symbol(ctx, in, *name);
    return include_scope{ chain_access::open_namespace(ctx.scope, *name), macros };
}

include_tables::include_tables(expand_ctx &ctx, include_scope tables)
    : m_ctx(ctx), m_including{ chain_access::enter(ctx.scope, tables.symbols), ctx.macros.enter(tables.macros) }
{
}

include_tables::~include_tables()
{
    chain_access::enter(m_ctx.scope, m_including.symbols);
    m_ctx.macros.enter(m_including.macros);
}

}
