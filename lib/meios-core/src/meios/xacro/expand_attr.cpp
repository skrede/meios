#include "structural_detail.h"
#include "substitution_detail.h"

#include "meios/xacro/value.h"
#include "meios/xacro/structural.h"
#include "meios/xacro/substitution.h"

#include "meios/diagnostic/expansion_error.h"
#include "meios/diagnostic/source_location.h"

#include "meios/expected.h"

#include <pugixml.hpp>

#include <string>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

std::string substitute_attr(expand_ctx &ctx, pugi::xml_node in, std::string_view raw,
                            const std::filesystem::path &document, bool &ok,
                            std::optional<std::size_t> attr_index)
{
    const source_location at = locate(ctx, in);
    const std::string_view host_text =
        ctx.origins.empty() ? std::string_view{} : ctx.origins.back().text;
    const expected<substitution, expansion_error> result =
        substitute_refined(raw, ctx.scope, ctx.sources, document, ctx.mode, ctx.backend,
                           ctx.session, ctx.log, at, in, host_text, attr_index);
    ok = result.has_value();
    if(!ok)
    {
        record_terminal(ctx, result.error());
        ctx.ok = false;
        return {};
    }
    return result->text;
}

// An exact ${...} binds the evaluated value with its type; anything else falls back to the
// text path, so mixed text still renders. A cleared ok is a terminal failure; an absent
// value with ok still set is a span a lenient policy retained, which binds as written.
value substitute_attr_value(expand_ctx &ctx, pugi::xml_node in, std::string_view raw,
                            const std::filesystem::path &document, bool &ok,
                            std::optional<std::size_t> attr_index)
{
    const std::optional<std::string_view> inner = exact_expression(raw);
    if(!inner)
    {
        const std::string text = substitute_attr(ctx, in, raw, document, ok, attr_index);
        return ok ? classify(text) : value{};
    }
    const expected<std::optional<value>, expansion_error> bound =
        substitute_exact(*inner, ctx.scope, ctx.sources, document, ctx.mode, ctx.backend,
                         ctx.session, ctx.log, locate(ctx, in));
    ok = bound.has_value();
    if(!ok)
    {
        record_terminal(ctx, bound.error());
        ctx.ok = false;
        return value{};
    }
    return *bound ? **bound : classify(raw);
}

}
