#include "substitution_detail.h"

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/substitution.h"
#include "meios/xacro/value_render.h"

#include "meios/io/source_stack.h"
#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/expansion_error.h"

#include "meios/expected.h"

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
    record_terminal(ctx, code, message);
    return std::nullopt;
}

void note_env_read(subst_ctx &ctx, std::string_view name)
{
    ctx.log.log(level::info, "read of environment variable \"" + std::string(name) + '"');
}

std::optional<std::string> cmd_find(subst_ctx &ctx, std::string_view rest)
{
    std::pair<std::string_view, std::string_view> parts = split_first(rest);
    if(parts.first.empty())
        return fail(ctx, diagnostic_code::unresolved_find, "$(find) requires a package name");
    const std::optional<resolved_asset> hit = ctx.sources.locate(parts.first, parts.second, ctx.log);
    if(!hit)
        return fail(ctx, diagnostic_code::unresolved_find,
                    "$(find " + std::string(parts.first) + ") did not resolve");
    return hit->path().string();
}

// A collection has no text form, so the argument is reported unresolved rather than written into
// the document as its own contents.
std::optional<std::string> bound_arg(subst_ctx &ctx, std::string_view name, const value &bound)
{
    std::optional<std::string> text = render_scalar(bound);
    if(text)
        return text;
    return fail(ctx, diagnostic_code::unresolved_arg,
                "$(arg " + std::string(name) + ") holds a " + std::string(kind_name(bound.kind()))
                    + ", which has no text form");
}

// The default is resolved only on the unset branch, and through the enclosing substitution so it
// is charged to the same load: a default that names an argument of its own nests here.
std::optional<std::string> arg_default(subst_ctx &ctx, std::string_view written)
{
    const expected<substitution, expansion_error> resolved = substitute_in(ctx, written);
    if(!resolved)
    {
        record_terminal(ctx, resolved.error());
        return std::nullopt;
    }
    return resolved->text;
}

std::optional<std::string> cmd_arg(subst_ctx &ctx, std::string_view rest)
{
    std::pair<std::string_view, std::string_view> parts = split_first(rest);
    if(parts.first.empty())
        return fail(ctx, diagnostic_code::unresolved_arg, "$(arg) requires an argument name");
    const std::optional<value> bound = ctx.scope.lookup(parts.first);
    if(bound)
        return bound_arg(ctx, parts.first, *bound);
    if(!parts.second.empty())
        return arg_default(ctx, parts.second);
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
