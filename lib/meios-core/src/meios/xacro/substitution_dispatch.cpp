#include "substitution_detail.h"

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/substitution.h"

#include "meios/io/source_stack.h"
#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <string>
#include <cstdlib>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios
{

namespace detail
{

namespace
{

std::pair<std::string_view, std::string_view> split_first(std::string_view text)
{
    std::size_t space = text.find_first_of(" \t");
    if(space == std::string_view::npos)
        return { text, {} };
    return { text.substr(0, space), trim(text.substr(space)) };
}

std::optional<std::string> fail(subst_ctx &ctx, diagnostic_code code, const std::string &message)
{
    ctx.log.log(level::error, code, ctx.at, message);
    return std::nullopt;
}

std::string binding_str(const binding &bound)
{
    if(std::holds_alternative<std::string>(bound))
        return std::get<std::string>(bound);
    return to_python_str(std::get<value>(bound));
}

void note_env_read(subst_ctx &ctx, std::string_view name)
{
    ctx.log.log(level::info, "read of environment variable \"" + std::string(name) + '"');
}

// Same ownership limit as a byte-backed mesh: the materialized file dies with the asset, so the
// substituted text would name a path that is already gone by the time the caller reads it.
std::optional<std::string> asset_path(subst_ctx &ctx, resolved_asset &&hit)
{
    if(hit.holds_bytes())
        return fail(ctx, diagnostic_code::unresolved_find,
                    "resolves to a byte-backed source; meios cannot yet substitute a path that "
                    "outlives the load");
    return hit.path().string();
}

std::optional<std::string> cmd_find(subst_ctx &ctx, std::string_view rest)
{
    std::pair<std::string_view, std::string_view> parts = split_first(rest);
    if(parts.first.empty())
        return fail(ctx, diagnostic_code::unresolved_find, "$(find) requires a package name");
    std::optional<resolved_asset> hit = ctx.sources.locate(parts.first, parts.second, ctx.log);
    if(!hit)
        return fail(ctx, diagnostic_code::unresolved_find,
                    "$(find " + std::string(parts.first) + ") did not resolve");
    return asset_path(ctx, std::move(*hit));
}

std::optional<std::string> cmd_arg(subst_ctx &ctx, std::string_view rest)
{
    std::pair<std::string_view, std::string_view> parts = split_first(rest);
    if(parts.first.empty())
        return fail(ctx, diagnostic_code::unresolved_arg, "$(arg) requires an argument name");
    std::optional<binding> bound = ctx.scope.lookup(parts.first);
    if(bound)
        return binding_str(*bound);
    if(!parts.second.empty())
    {
        substitution resolved = substitute(parts.second, ctx.scope, ctx.sources, ctx.document,
                                           ctx.mode, ctx.backend, ctx.log, ctx.at);
        if(!resolved.ok)
            return std::nullopt;
        return resolved.text;
    }
    return fail(ctx, diagnostic_code::unresolved_arg,
                "$(arg " + std::string(parts.first) + ") is unset and has no default");
}

std::optional<std::string> cmd_env(subst_ctx &ctx, std::string_view rest)
{
    std::string_view name = split_first(rest).first;
    if(name.empty())
        return fail(ctx, diagnostic_code::unresolved_env, "$(env) requires a variable name");
    note_env_read(ctx, name);
    const char *value = std::getenv(std::string(name).c_str());
    if(value == nullptr)
        return fail(ctx, diagnostic_code::unresolved_env,
                    "$(env " + std::string(name) + ") is not set in the environment");
    return std::string(value);
}

std::optional<std::string> cmd_optenv(subst_ctx &ctx, std::string_view rest)
{
    std::pair<std::string_view, std::string_view> parts = split_first(rest);
    if(parts.first.empty())
        return fail(ctx, diagnostic_code::unresolved_env, "$(optenv) requires a variable name");
    note_env_read(ctx, parts.first);
    const char *value = std::getenv(std::string(parts.first).c_str());
    if(value != nullptr)
        return std::string(value);
    return std::string(parts.second);
}

}

std::optional<std::string> dispatch(subst_ctx &ctx, std::string_view inner)
{
    std::pair<std::string_view, std::string_view> parts = split_first(trim(inner));
    std::string_view cmd = parts.first;
    if(cmd == "find")
        return cmd_find(ctx, parts.second);
    if(cmd == "arg")
        return cmd_arg(ctx, parts.second);
    if(cmd == "eval")
        return eval_expr(ctx, parts.second);
    if(cmd == "dirname")
        return ctx.document.parent_path().string();
    if(cmd == "env")
        return cmd_env(ctx, parts.second);
    if(cmd == "optenv")
        return cmd_optenv(ctx, parts.second);
    return fail(ctx, diagnostic_code::unknown_substitution,
                "unknown substitution command $(" + std::string(cmd) + ")");
}

}

}
