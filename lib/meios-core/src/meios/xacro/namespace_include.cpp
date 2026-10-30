#include "scope_chain.h"
#include "structural_detail.h"

#include "meios/xacro/structural.h"

#include <pugixml.hpp>

#include <string>
#include <cstddef>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

// The reference tests the attribute as written, so an empty one reads the file into the including
// table, and it evaluates a namespace in the including table before it reads the filename.
std::size_t include_table(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document, bool &ok)
{
    const std::string_view raw = in.attribute("ns").value();
    ok = true;
    if(raw.empty())
        return chain_access::cursor(ctx.scope);
    const std::string authored = strip_authored_markers(std::string(raw));
    const std::string name = substitute_attr(ctx, in, authored, document, ok, attr_dom_index(in, "ns"));
    if(!ok)
        return chain_access::cursor(ctx.scope);
    return chain_access::open_namespace(ctx.scope, name);
}

include_tables::include_tables(expand_ctx &ctx, std::size_t symbols)
    : m_ctx(ctx), m_including(chain_access::enter(ctx.scope, symbols))
{
}

include_tables::~include_tables()
{
    chain_access::enter(m_ctx.scope, m_including);
}

}
