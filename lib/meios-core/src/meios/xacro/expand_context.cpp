#include "expand_context.h"

#include "meios/detail/text_location.h"

#include "meios/diagnostic/level.h"

#include <pugixml.hpp>

#include <string>
#include <memory>
#include <utility>
#include <filesystem>

namespace meios::detail
{

expand_ctx::expand_ctx(eval_scope &s, source_stack &src, const expansion_limits &lim,
                       eval_policy policy, const std::shared_ptr<evaluator_handle> &inject,
                       log_sink &lg)
    : scope(s), sources(src), limits(lim), log(lg), mode(policy), backend(inject), counters(),
      macros(), blocks(), include_stack(), owned(), owned_text(), origins(), prop_frames(),
      param_saves(), terminal(), ok(true)
{
}

void record_terminal(expand_ctx &ctx, const source_location &loc, diagnostic_code code,
                     const std::string &message)
{
    if(!ctx.terminal)
        ctx.terminal = expansion_error{ loc, message, code, std::nullopt };
}

bool expand_ctx::charge_work(pugi::xml_node in)
{
    if(!ok)
        return false;
    if(++counters.work > limits.work)
    {
        const std::string message = "expansion budget exceeded: work limit of "
                                  + std::to_string(limits.work) + " units reached";
        record_terminal(*this, locate(*this, in), diagnostic_code::expansion_budget_exceeded,
                        message);
        log.log(level::error, diagnostic_code::expansion_budget_exceeded, locate(*this, in),
                message);
        ok = false;
        return false;
    }
    return true;
}

bool expand_ctx::charge_output(pugi::xml_node in)
{
    if(!ok)
        return false;
    if(++counters.output_nodes > limits.output_nodes)
    {
        const std::string message = "expansion budget exceeded: output-node limit of "
                                  + std::to_string(limits.output_nodes) + " nodes reached";
        record_terminal(*this, locate(*this, in), diagnostic_code::expansion_budget_exceeded,
                        message);
        log.log(level::error, diagnostic_code::expansion_budget_exceeded, locate(*this, in),
                message);
        ok = false;
        return false;
    }
    return true;
}

pugi::xml_document &expand_ctx::park()
{
    owned.push_back(std::make_unique<pugi::xml_document>());
    return *owned.back();
}

bool fail(expand_ctx &ctx, const source_location &loc, diagnostic_code code,
          const std::string &message)
{
    record_terminal(ctx, loc, code, message);
    ctx.log.log(level::error, code, loc, message);
    ctx.ok = false;
    return false;
}

bool fail(expand_ctx &ctx, pugi::xml_node in, diagnostic_code code, const std::string &message)
{
    return fail(ctx, locate(ctx, in), code, message);
}

bool fail(expand_ctx &ctx, const source_location &loc, diagnostic_code code,
          const operation_failure &cause, const std::string &message)
{
    if(!ctx.terminal)
        ctx.terminal = expansion_error{ loc, message, code, cause };
    ctx.log.log(level::error, code, loc, cause, message);
    ctx.ok = false;
    return false;
}

// A typed xacro emit always anchors on the hosting node's real position; with no
// origin on the stack, or a node pugixml never gave a buffer offset, fall back to the
// document path at 0:0 rather than dropping to a code-less, locationless emit.
source_location locate(const expand_ctx &ctx, pugi::xml_node in)
{
    if(ctx.origins.empty() || in.offset_debug() < 0)
        return source_location{ ctx.origins.empty() ? std::filesystem::path{} : ctx.origins.back().path,
                                0, 0 };
    return offset_location(ctx.origins.back().text, in.offset_debug(), ctx.origins.back().path);
}

}
