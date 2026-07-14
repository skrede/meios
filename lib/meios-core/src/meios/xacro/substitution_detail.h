#ifndef HPP_GUARD_MEIOS_XACRO_SUBSTITUTION_DETAIL_H
#define HPP_GUARD_MEIOS_XACRO_SUBSTITUTION_DETAIL_H

#include "meios/xacro/eval_scope.h"
#include "meios/xacro/core_evaluator.h"

#include "meios/diagnostic/log_sink.h"

#include <string>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios
{

class source_stack;

namespace detail
{

struct subst_ctx
{
    subst_ctx(log_sink &sink, source_stack &pkg_sources, const eval_scope &names,
              const std::filesystem::path &doc)
        : log(sink), sources(pkg_sources), scope(names), evaluator(), document(doc)
    {
    }

    log_sink &log;
    source_stack &sources;
    const eval_scope &scope;
    core_evaluator evaluator;
    const std::filesystem::path &document;
};

std::optional<std::string> eval_expr(subst_ctx &ctx, std::string_view expression);

std::optional<std::string> dispatch(subst_ctx &ctx, std::string_view inner);

}

}

#endif
