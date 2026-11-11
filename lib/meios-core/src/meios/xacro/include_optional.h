#ifndef HPP_GUARD_MEIOS_XACRO_INCLUDE_OPTIONAL_H
#define HPP_GUARD_MEIOS_XACRO_INCLUDE_OPTIONAL_H

#include "expand_context.h"

#include "meios/diagnostic/operation_failure.h"

#include <pugixml.hpp>

#include <string>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios
{
class source_stack;
}

namespace meios::detail
{

// Marks whether the include being routed is optional, and restores the enclosing mark on whichever
// path the routing returns; the included file expands under a cleared mark, so an include it writes
// is judged on its own attribute.
class optional_include_scope
{
public:
    optional_include_scope(expand_ctx &ctx, bool optional);
    optional_include_scope(const optional_include_scope &)            = delete;
    optional_include_scope &operator=(const optional_include_scope &) = delete;
    ~optional_include_scope();

private:
    expand_ctx &m_ctx;
    bool m_enclosing;
};

// Absent where the attribute refused, which the expansion has recorded; an absent attribute reads false.
std::optional<bool> include_optional(expand_ctx &ctx, pugi::xml_node in, const std::filesystem::path &document);

// Under an optional include, notes that the file was not read and answers true having included
// nothing; otherwise refuses as an unresolved include with the message given.
bool missing_include(expand_ctx &ctx, pugi::xml_node in, const std::string &message);
bool missing_include(expand_ctx &ctx, pugi::xml_node in, const std::string &message, const operation_failure &cause, std::string_view reason);

// A package-qualified miss is a missing file only where a source holds the package, and only where no
// source that names paths found something at the spelling that it refused to serve.
bool missing_from_sources(source_stack &sources, const std::string &package, const std::string &normalized);

}

#endif
