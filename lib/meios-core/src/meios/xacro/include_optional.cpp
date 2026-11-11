#include "boolean_value.h"
#include "include_optional.h"
#include "structural_detail.h"

#include "meios/io/source_stack.h"
#include "meios/io/source_handle.h"
#include "meios/io/beside_containment.h"

#include "meios/xacro/value.h"
#include "meios/xacro/structural.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <pugixml.hpp>

#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <algorithm>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

namespace
{

bool skip_include(expand_ctx &ctx, pugi::xml_node in, std::string_view reason)
{
    const std::string filename = strip_authored_markers(in.attribute("filename").value());
    ctx.log.log(level::info, locate(ctx, in), "optional include \"" + filename + "\" was not read: " + std::string(reason) + "; nothing was included");
    return true;
}

bool holds_package(source_handle &layer, const std::string &package)
{
    if(layer.answers(package, ""))
        return true;
    const std::vector<std::string> held = layer.packages();
    return std::ranges::find(held, package) != held.end();
}

// The layer has already recorded its refusal of a spelling that escapes its root, so the spelling
// must stay inside the package's directory for the miss to be a file that is not there.
bool stays_in_package(source_handle &layer, const std::string &package, const std::string &normalized)
{
    const std::optional<std::filesystem::path> directory = layer.names_paths() ? layer.path_of(package, "") : std::nullopt;
    if(!directory)
        return true;
    const contained_read_result decided = contain_under(*directory, *directory / normalized);
    return decided && *decided;
}

}

optional_include_scope::optional_include_scope(expand_ctx &ctx, bool optional)
        : m_ctx(ctx)
        , m_enclosing(std::exchange(ctx.optional_include, optional))
{
}

optional_include_scope::~optional_include_scope()
{
    m_ctx.optional_include = m_enclosing;
}

std::optional<bool> include_optional(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document)
{
    if(!in.attribute("optional"))
        return false;
    const std::string authored = strip_authored_markers(in.attribute("optional").value());
    bool ok                    = true;
    const value read           = substitute_condition_value(ctx, in, authored, document, ok, attr_dom_index(in, "optional"));
    if(!ok)
        return std::nullopt;
    const std::optional<bool> optional = get_boolean_value(read);
    if(!optional)
        fail(ctx, in, diagnostic_code::xacro_structural_error, "xacro:include optional=\"" + authored + "\" evaluates to \"" + read.text().value_or("") + "\", which is not a boolean");
    return optional;
}

bool missing_include(expand_ctx &ctx, pugi::xml_node in, const std::string &message)
{
    if(ctx.optional_include)
        return skip_include(ctx, in, "it was not found");
    return fail(ctx, in, diagnostic_code::unresolved_include, message);
}

bool missing_include(expand_ctx &ctx, pugi::xml_node in, const std::string &message, const operation_failure &cause, std::string_view reason)
{
    if(ctx.optional_include)
        return skip_include(ctx, in, reason);
    return fail(ctx, locate(ctx, in), diagnostic_code::unresolved_include, cause, message);
}

bool missing_from_sources(source_stack &sources, const std::string &package, const std::string &normalized)
{
    bool held = package.empty();
    for(source_handle &layer : sources.layers())
    {
        if(!holds_package(layer, package))
            continue;
        if(!stays_in_package(layer, package, normalized))
            return false;
        held = true;
    }
    return held;
}

}
