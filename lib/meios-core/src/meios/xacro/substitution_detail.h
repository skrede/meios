#ifndef HPP_GUARD_MEIOS_XACRO_SUBSTITUTION_DETAIL_H
#define HPP_GUARD_MEIOS_XACRO_SUBSTITUTION_DETAIL_H

#include "meios/xacro/eval_scope.h"
#include "meios/xacro/eval_policy.h"
#include "meios/xacro/core_evaluator.h"
#include "meios/xacro/evaluator_handle.h"

#include "meios/diagnostic/log_sink.h"

#include <cctype>
#include <string>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios
{

class source_stack;

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
                                            log_sink &log);

    eval_failure_kind last_failure_kind() const { return m_kind; }

private:
    eval_failure_kind m_kind;
};

struct subst_ctx
{
    subst_ctx(log_sink &sink, source_stack &pkg_sources, const eval_scope &names,
              const std::filesystem::path &doc, eval_policy policy, evaluator_handle *inject)
        : log(sink), sources(pkg_sources), scope(names), core(), document(doc),
          mode(policy), backend(inject), last_kind(eval_failure_kind::none)
    {
    }

    subst_ctx(log_sink &sink, source_stack &pkg_sources, const eval_scope &names,
              const std::filesystem::path &doc)
        : subst_ctx(sink, pkg_sources, names, doc, eval_policy::fail, nullptr)
    {
    }

    log_sink &log;
    source_stack &sources;
    const eval_scope &scope;
    core_text_evaluator core;
    const std::filesystem::path &document;
    eval_policy mode;
    evaluator_handle *backend;
    eval_failure_kind last_kind;
};

std::optional<std::string> eval_expr(subst_ctx &ctx, std::string_view expression);

std::optional<std::string> dispatch(subst_ctx &ctx, std::string_view inner);

}

}

#endif
