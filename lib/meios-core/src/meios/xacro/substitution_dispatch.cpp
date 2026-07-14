#include "substitution_detail.h"

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/core_evaluator.h"

#include "meios/io/materialize.h"
#include "meios/io/source_stack.h"
#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <string>
#include <cctype>
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

std::string_view trim(std::string_view text)
{
    std::size_t begin = text.find_first_not_of(" \t");
    if(begin == std::string_view::npos)
        return {};
    return text.substr(begin, text.find_last_not_of(" \t") - begin + 1);
}

std::pair<std::string_view, std::string_view> split_first(std::string_view text)
{
    std::size_t space = text.find_first_of(" \t");
    if(space == std::string_view::npos)
        return { text, {} };
    return { text.substr(0, space), trim(text.substr(space)) };
}

bool is_identifier(std::string_view text)
{
    if(text.empty() || (!std::isalpha(static_cast<unsigned char>(text[0])) && text[0] != '_'))
        return false;
    for(char c : text)
        if(!std::isalnum(static_cast<unsigned char>(c)) && c != '_')
            return false;
    return true;
}

std::optional<std::string> fail(subst_ctx &ctx, const std::string &message)
{
    ctx.log.log(level::error, message);
    return std::nullopt;
}

std::string binding_str(const binding &bound)
{
    if(std::holds_alternative<std::string>(bound))
        return std::get<std::string>(bound);
    return to_python_str(std::get<value>(bound));
}

std::optional<std::string> string_property(subst_ctx &ctx, std::string_view name)
{
    if(!is_identifier(name))
        return std::nullopt;
    std::optional<binding> bound = ctx.scope.lookup(name);
    if(bound && std::holds_alternative<std::string>(*bound))
        return std::get<std::string>(*bound);
    return std::nullopt;
}

void note_env_read(subst_ctx &ctx, std::string_view name)
{
    ctx.log.log(level::info, "read of environment variable \"" + std::string(name) + '"');
}

std::optional<std::string> asset_path(subst_ctx &ctx, resolved_asset &&hit)
{
    resolved_asset located = hit.holds_bytes() ? materialize(std::move(hit), ctx.log)
                                               : std::move(hit);
    return located.path().string();
}

std::optional<std::string> cmd_find(subst_ctx &ctx, std::string_view rest)
{
    std::pair<std::string_view, std::string_view> parts = split_first(rest);
    if(parts.first.empty())
        return fail(ctx, "$(find) requires a package name");
    std::optional<resolved_asset> hit = ctx.sources.locate(parts.first, parts.second, ctx.log);
    if(!hit)
        return fail(ctx, "$(find " + std::string(parts.first) + ") did not resolve");
    return asset_path(ctx, std::move(*hit));
}

std::optional<std::string> cmd_arg(subst_ctx &ctx, std::string_view rest)
{
    std::pair<std::string_view, std::string_view> parts = split_first(rest);
    if(parts.first.empty())
        return fail(ctx, "$(arg) requires an argument name");
    std::optional<binding> bound = ctx.scope.lookup(parts.first);
    if(bound)
        return binding_str(*bound);
    if(!parts.second.empty())
        return std::string(parts.second);
    return fail(ctx, "$(arg " + std::string(parts.first) + ") is unset and has no default");
}

std::optional<std::string> cmd_env(subst_ctx &ctx, std::string_view rest)
{
    std::string_view name = split_first(rest).first;
    if(name.empty())
        return fail(ctx, "$(env) requires a variable name");
    note_env_read(ctx, name);
    const char *value = std::getenv(std::string(name).c_str());
    if(value == nullptr)
        return fail(ctx, "$(env " + std::string(name) + ") is not set in the environment");
    return std::string(value);
}

std::optional<std::string> cmd_optenv(subst_ctx &ctx, std::string_view rest)
{
    std::pair<std::string_view, std::string_view> parts = split_first(rest);
    if(parts.first.empty())
        return fail(ctx, "$(optenv) requires a variable name");
    note_env_read(ctx, parts.first);
    const char *value = std::getenv(std::string(parts.first).c_str());
    if(value != nullptr)
        return std::string(value);
    return std::string(parts.second);
}

}

std::optional<std::string> eval_expr(subst_ctx &ctx, std::string_view expression)
{
    std::string_view expr = trim(expression);
    std::optional<std::string> direct = string_property(ctx, expr);
    if(direct)
        return direct;
    value result = ctx.evaluator.eval(expr, ctx.scope, ctx.log);
    if(ctx.evaluator.failed())
        return std::nullopt;
    return to_python_str(result);
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
    return fail(ctx, "unknown substitution command $(" + std::string(cmd) + ")");
}

}

}
