#ifndef HPP_GUARD_MEIOS_XACRO_SUBSTITUTION_DETAIL_H
#define HPP_GUARD_MEIOS_XACRO_SUBSTITUTION_DETAIL_H

#include "meios/xacro/eval_scope.h"
#include "meios/xacro/eval_policy.h"
#include "meios/xacro/core_evaluator.h"
#include "meios/xacro/evaluator_handle.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/source_location.h"

#include <pugixml.hpp>

#include <cctype>
#include <memory>
#include <string>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios
{

class source_stack;
class evaluator_handle;
struct substitution;

namespace detail
{

inline std::string_view trim(std::string_view text)
{
    std::size_t begin = text.find_first_not_of(" \t");
    if(begin == std::string_view::npos)
        return {};
    return text.substr(begin, text.find_last_not_of(" \t") - begin + 1);
}

inline bool is_identifier(std::string_view text)
{
    if(text.empty() || (!std::isalpha(static_cast<unsigned char>(text[0])) && text[0] != '_'))
        return false;
    for(char c : text)
        if(!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
            return false;
    return true;
}

// The always-on backend: it renders an expression through the core evaluator and,
// on a failure, reports whether the core simply lacks the feature (unsupported) or
// hit a genuine fault (error). eval-python provides a text_evaluator of the same
// shape, so the injection seam and the default path are one interface.
class core_text_evaluator
{
public:
    core_text_evaluator() : m_kind(eval_failure_kind::none) {}

    std::optional<std::string> eval_to_text(std::string_view expr, const eval_scope &scope,
                                            log_sink &log, const source_location &at = {});

    eval_failure_kind last_failure_kind() const { return m_kind; }

private:
    eval_failure_kind m_kind;
};

struct subst_ctx
{
    subst_ctx(log_sink &sink, source_stack &pkg_sources, const eval_scope &names,
              const std::filesystem::path &doc, eval_policy policy,
              const std::shared_ptr<evaluator_handle> &inject, const source_location &anchor = {})
        : log(sink), sources(pkg_sources), scope(names), core(), document(doc), at(anchor),
          node_anchor(anchor), mode(policy), backend(inject), last_kind(eval_failure_kind::none)
    {
    }

    subst_ctx(log_sink &sink, source_stack &pkg_sources, const eval_scope &names,
              const std::filesystem::path &doc)
        : subst_ctx(sink, pkg_sources, names, doc, eval_policy::fail, {})
    {
    }

    log_sink &log;
    source_stack &sources;
    const eval_scope &scope;
    core_text_evaluator core;
    const std::filesystem::path &document;
    source_location at;
    // The immutable node-anchored floor: `at`'s column may be refined to a failing
    // token, but a degrade always falls back to this, never to a prior span's column.
    source_location node_anchor;
    eval_policy mode;
    std::shared_ptr<evaluator_handle> backend;
    eval_failure_kind last_kind;
    // Optional forward-scan refinement inputs, empty for the public substitute() path:
    // the hosting element, its raw document text, and the attribute's DOM index.
    pugi::xml_node host{};
    std::string_view host_text{};
    std::optional<std::size_t> attr_index{};
};

std::optional<std::string> eval_expr(subst_ctx &ctx, std::string_view expression);

std::optional<std::string> dispatch(subst_ctx &ctx, std::string_view inner);

// Internal substitute that carries the forward-scan refinement inputs so an
// attribute-hosted failing token's column can refine the node anchor. The public
// substitute() overloads keep the node anchor and do not widen for this.
substitution substitute_refined(std::string_view raw, const eval_scope &scope,
                                source_stack &sources, const std::filesystem::path &document,
                                eval_policy policy,
                                const std::shared_ptr<evaluator_handle> &backend, log_sink &log,
                                const source_location &at, pugi::xml_node host,
                                std::string_view host_text, std::optional<std::size_t> attr_index);

}

}

#endif
