#ifndef HPP_GUARD_MEIOS_XACRO_DISPATCH_VIEW_H
#define HPP_GUARD_MEIOS_XACRO_DISPATCH_VIEW_H

#include <pugixml.hpp>

#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <string_view>

namespace meios::detail
{

// A rename dispatches the authored node again under the name it evaluated, rather than renaming or
// cloning it: an input document is parked and a macro body reused, and a clone has no file:line.
// The attribute a rename consumed reads as absent from then on.
struct dispatch_view
{
    pugi::xml_node node;
    std::string name;
    std::vector<std::string_view> dropped;
};

inline dispatch_view authored_view(pugi::xml_node node)
{
    return dispatch_view{node, node.name(), {}};
}

inline bool consumed(const dispatch_view &view, std::string_view attr)
{
    return std::ranges::find(view.dropped, attr) != view.dropped.end();
}

inline pugi::xml_attribute view_attribute(const dispatch_view &view, std::string_view name)
{
    return consumed(view, name) ? pugi::xml_attribute{} : view.node.attribute(std::string(name).c_str());
}

inline dispatch_view renamed(const dispatch_view &view, std::string name, std::string_view attr)
{
    dispatch_view next{view.node, std::move(name), view.dropped};
    next.dropped.push_back(attr);
    return next;
}

}

#endif
