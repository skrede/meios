#ifndef HPP_GUARD_MEIOS_XACRO_NAMESPACE_LIFT_H
#define HPP_GUARD_MEIOS_XACRO_NAMESPACE_LIFT_H

#include "expand_context.h"

#include <pugixml.hpp>

#include <string>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

// An attribute in the xacro namespace never reaches the output and is never evaluated.
bool xacro_namespace_attribute(std::string_view name);

// The declaration of the xacro namespace is evaluated like any attribute of the element holding it,
// so a failing one refuses, but it is never written to the output nor lifted.
bool xacro_namespace_declaration(std::string_view name);

// Makes onto the element that declarations lifted out of an included root or a macro definition
// land on, and the enclosing one current again on whichever path the walk returns. An empty handle
// drops them: a conditional and a block inserted by its content leave no element of their own.
class lift_target
{
public:
    lift_target(expand_ctx &ctx, pugi::xml_node onto);
    lift_target(const lift_target &)            = delete;
    lift_target &operator=(const lift_target &) = delete;
    ~lift_target();

private:
    expand_ctx &m_ctx;
    pugi::xml_node m_enclosing;
};

// Reads the attributes of an included root or a macro definition, an element the output drops: each
// one, in document order, is evaluated and its result discarded, except one in the xacro namespace
// and the one named evaluated_elsewhere (a definition's params, whose defaults every call evaluates
// already). Each namespace declaration other than the xacro one is then set on the current lift
// target; a prefix the target already binds to another value keeps that value, and the conflict is
// logged as a warning located at from. False where an attribute refused to evaluate.
bool read_wrapper_attributes(expand_ctx &ctx, pugi::xml_node from, const std::filesystem::path &document, std::string_view evaluated_elsewhere = {});

// The evaluated target namespace written on the top document's root, absent where the attribute is
// absent or written empty. It is read before anything under the root, as the root's own attributes are.
std::optional<std::string> target_namespace(expand_ctx &ctx, pugi::xml_node root, pugi::xml_attribute written, const std::filesystem::path &document, bool &ok);

void declare_default_namespace(pugi::xml_node root, const std::string &uri);

}

#endif
