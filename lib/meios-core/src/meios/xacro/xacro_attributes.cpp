#include "xacro_attributes.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <pugixml.hpp>

#include <array>
#include <string>
#include <cstddef>
#include <algorithm>
#include <string_view>

namespace meios::detail
{

namespace
{

// A declared argument without a default is a required argument, so `default` is known here but
// not required.
constexpr std::array<attribute_row, 10> rows{{
        {"xacro:include", "filename", "ns,optional", true},
        {"xacro:macro", "name", "params", true},
        {"xacro:property", "name", "value,default,remove,scope,lazy_eval", true},
        {"xacro:arg", "name", "default", true},
        {"xacro:insert_block", "name", "", true},
        {"xacro:if", "value", "", true},
        {"xacro:unless", "value", "", true},
        {"xacro:call", "macro", "", false},
        {"xacro:element", "xacro:name", "", false},
        {"xacro:attribute", "name,value", "", false},
}};

template <typename Predicate>
std::string_view first_name(std::string_view list, Predicate matches)
{
    while(!list.empty())
    {
        const std::size_t comma = std::min(list.find(','), list.size());
        const std::string_view name = list.substr(0, comma);
        if(matches(name))
            return name;
        list.remove_prefix(std::min(comma + 1, list.size()));
    }
    return {};
}

bool listed(std::string_view list, std::string_view name)
{
    return !first_name(list, [name](std::string_view one) { return one == name; }).empty();
}

bool present(pugi::xml_node in, std::string_view name)
{
    for(pugi::xml_attribute attr : in.attributes())
        if(name == attr.name())
            return true;
    return false;
}

bool known(const attribute_row &row, std::string_view name, std::string_view dropped)
{
    return name == dropped || name.starts_with("xmlns:") || listed(row.required, name) || listed(row.optional, name);
}

}

std::span<const attribute_row> attribute_rows()
{
    return rows;
}

bool check_attributes(expand_ctx &ctx, pugi::xml_node in, std::string_view tag, std::string_view dropped)
{
    const auto row = std::ranges::find(rows, tag, &attribute_row::tag);
    if(row == rows.end())
        return true;
    const std::string_view missing = first_name(row->required, [in](std::string_view name) { return !present(in, name); });
    if(!missing.empty())
        return fail(ctx, in, diagnostic_code::xacro_structural_error, '<' + std::string(tag) + "> needs a " + std::string(missing) + " attribute");
    if(!row->warns)
        return true;
    for(pugi::xml_attribute attr : in.attributes())
        if(!known(*row, attr.name(), dropped))
            ctx.log.log(level::warn, diagnostic_code::unknown_attribute, locate(ctx, in),
                        '<' + std::string(tag) + "> carries an attribute '" + attr.name() + "' it does not read");
    return true;
}

}
