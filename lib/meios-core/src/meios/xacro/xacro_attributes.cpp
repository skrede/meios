#include "xacro_attributes.h"
#include "namespace_lift.h"

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

bool present(const dispatch_view &view, std::string_view name)
{
    for(pugi::xml_attribute attr : view.node.attributes())
        if(name == attr.name())
            return !consumed(view, name);
    return false;
}

bool known(const attribute_row &row, std::string_view name, const dispatch_view &view)
{
    return consumed(view, name) || name.starts_with("xmlns:") || listed(row.required, name) || listed(row.optional, name);
}

// A macro definition evaluates its other attributes on each call, so a failing one refuses though
// its value reaches nothing; one in the xacro namespace is skipped unread there as everywhere.
std::string unused_attribute(const dispatch_view &view, std::string_view name)
{
    const bool evaluated        = view.name == "xacro:macro" && !xacro_namespace_attribute(name);
    const std::string_view fate = evaluated ? "' it evaluates on each call but does not use" : "' it does not read";
    std::string message         = '<' + view.name + "> carries an attribute '" + std::string(name);
    return message.append(fate);
}

}

std::span<const attribute_row> attribute_rows()
{
    return rows;
}

bool check_attributes(expand_ctx &ctx, const dispatch_view &view)
{
    const auto row = std::ranges::find(rows, std::string_view(view.name), &attribute_row::tag);
    if(row == rows.end())
        return true;
    const std::string_view missing = first_name(row->required, [&view](std::string_view name) { return !present(view, name); });
    if(!missing.empty())
        return fail(ctx, view.node, diagnostic_code::xacro_structural_error, '<' + view.name + "> needs a " + std::string(missing) + " attribute");
    if(!row->warns)
        return true;
    for(pugi::xml_attribute attr : view.node.attributes())
        if(!known(*row, attr.name(), view))
            ctx.log.log(level::warn, locate(ctx, view.node), unused_attribute(view, attr.name()));
    return true;
}

}
