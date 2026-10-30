#ifndef HPP_GUARD_MEIOS_XACRO_DEFERRED_READ_H
#define HPP_GUARD_MEIOS_XACRO_DEFERRED_READ_H

#include "macro_tables.h"

#include <pugixml.hpp>

#include <cstddef>
#include <optional>
#include <filesystem>

namespace meios::detail
{

struct expand_ctx;

// Where a deferred text was written: the property element, the attribute that holds the text, and
// the file the element came from, so its evaluation reports a refusal inside that text.
struct deferred_origin
{
    pugi::xml_node host;
    std::optional<std::size_t> attr_index;
    emit_origin origin;
    std::filesystem::path document;
};

std::size_t record_origin(expand_ctx &ctx, pugi::xml_node host, const char *attribute,
                          const std::filesystem::path &document);

// Lets the scope evaluate deferred texts for as long as one expansion runs. The scope outlives the
// context the evaluation needs, so every return from expand has to detach it, and it leaves the
// document table current with no table opened beneath it.
class resolver_guard
{
public:
    explicit resolver_guard(expand_ctx &ctx);
    resolver_guard(const resolver_guard &) = delete;
    resolver_guard &operator=(const resolver_guard &) = delete;
    ~resolver_guard();

private:
    expand_ctx &m_ctx;
};

}

#endif
