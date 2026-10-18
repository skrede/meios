#include "lexer.h"
#include "reference_names.h"
#include "argument_conversion.h"
#include "substitution_detail.h"

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/value_render.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <string>
#include <cstddef>
#include <optional>
#include <string_view>

namespace meios::detail
{

namespace
{

// What upstream's command resolves before an argument beside the mathematics names (xacro 2.1.1,
// substitution_args._eval): its own helpers and four boolean spellings, so an argument named like
// one of these is shadowed there.
constexpr std::string_view command_names[] = {"arg", "dirname", "env", "optenv", "find", "true", "false", "True", "False"};

void bind_reference_names(eval_scope &over)
{
    over.set("true", value{true});
    over.set("True", value{true});
    over.set("false", value{false});
    over.set("False", value{false});
    over.set("e", *value::make_real(2.718281828459045));
    over.set("tau", *value::make_real(6.283185307179586));
}

std::optional<std::string> refuse(subst_ctx &ctx, const std::string &message)
{
    ctx.log.log(level::error, diagnostic_code::expression_error, ctx.at, message);
    record_terminal(ctx, diagnostic_code::expression_error, message);
    return std::nullopt;
}

// Empty when the name is bound, or names no argument the command reads; otherwise why it is not.
std::optional<std::string> bind_read(const eval_scope &scope, eval_scope &over, std::string_view name)
{
    const std::optional<value> bound = scope.argument(name);
    if(!bound || listed(command_names, name) || listed(mathematics_names, name))
        return std::nullopt;
    const std::string quoted              = "the evaluation command reads the argument '" + std::string(name) + "'";
    const std::optional<std::string> text = render_scalar(*bound);
    if(!text)
        return quoted + ", which holds a " + std::string(kind_name(bound->kind())) + " and has no text form to convert";
    const std::optional<value> read = converted_argument(*text);
    if(!read)
        return quoted + ", whose number " + *text + " is outside the range this evaluator carries";
    over.set(name, *read);
    return std::nullopt;
}

}

// Upstream refuses a double underscore anywhere in the body before it evaluates anything, and
// evaluates the rest over its argument table, with no property visible.
std::optional<std::string> eval_command(subst_ctx &ctx, std::string_view body)
{
    if(body.find("__") != std::string_view::npos)
        return refuse(ctx, "the evaluation command's body holds a double underscore '__', which the reference refuses in it");
    eval_scope over;
    bind_reference_names(over);
    for(const token &one : tokenize(body))
    {
        const std::optional<std::string> refused = one.kind == token_kind::name ? bind_read(ctx.scope, over, one.text) : std::nullopt;
        if(refused)
            return refuse(ctx, *refused);
    }
    return eval_expr(ctx, body, over);
}

}
