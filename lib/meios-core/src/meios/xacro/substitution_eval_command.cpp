#include "lexer.h"
#include "reference_names.h"
#include "substitution_detail.h"

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/value_render.h"
#include "meios/xacro/detail/numeric.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <cctype>
#include <string>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <algorithm>
#include <string_view>

namespace meios::detail
{

namespace
{

// What upstream's command resolves before an argument beside the mathematics names (xacro 2.1.1,
// substitution_args._eval): its own helpers and four boolean spellings, so an argument named like
// one of these is shadowed there.
constexpr std::string_view command_names[] = {"arg", "dirname", "env", "optenv", "find", "true", "false", "True", "False"};

std::size_t sign(std::string_view text, std::size_t at, std::string &out)
{
    if(at >= text.size() || (text[at] != '+' && text[at] != '-'))
        return at;
    if(text[at] == '-')
        out += '-';
    return at + 1;
}

// Python's int() and float() read single underscores between digits, which from_chars does not.
std::size_t digit_run(std::string_view text, std::size_t at, std::string &out)
{
    if(at >= text.size() || !std::isdigit(static_cast<unsigned char>(text[at])))
        return std::string_view::npos;
    out += text[at++];
    for(std::size_t next = at; at < text.size(); at = next + 1)
    {
        next = text[at] == '_' ? at + 1 : at;
        if(next >= text.size() || !std::isdigit(static_cast<unsigned char>(text[next])))
            break;
        out += text[next];
    }
    return at;
}

std::optional<value> integer_spelling(std::string_view text)
{
    std::string digits;
    if(digit_run(text, sign(text, 0, digits), digits) != text.size())
        return value{std::string(text)};
    bool ok                   = false;
    const std::int64_t number = parse_int(digits, ok);
    return ok ? std::optional<value>(value{number}) : std::nullopt;
}

std::optional<value> real_spelling(std::string_view text)
{
    std::string digits;
    const std::size_t start = sign(text, 0, digits);
    const std::size_t whole = digit_run(text, start, digits);
    std::size_t at          = whole == std::string_view::npos ? start : whole;
    if(at >= text.size() || text[at] != '.')
        return value{std::string(text)};
    digits += '.';
    const std::size_t fraction = digit_run(text, at + 1, digits);
    at                         = fraction == std::string_view::npos ? at + 1 : fraction;
    if(at < text.size() && (text[at] == 'e' || text[at] == 'E'))
        at = digit_run(text, sign(text, at + 1, digits += 'e'), digits);
    if(at != text.size() || (whole == std::string_view::npos && fraction == std::string_view::npos))
        return value{std::string(text)};
    bool ok             = false;
    const double number = parse_double(digits, ok);
    return ok ? value::make_real(number) : std::nullopt;
}

std::string lowered(std::string_view text)
{
    std::string out(text);
    std::ranges::transform(out, out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// Upstream's convert_value(text, 'auto'): a real where the text holds a '.', an integer otherwise,
// then the two boolean spellings in any case, then the text itself. A number with no value here
// answers nothing rather than becoming text.
std::optional<value> converted(std::string_view text)
{
    const std::size_t begin         = text.find_first_not_of(" \t\n\v\f\r");
    const std::string_view number   = begin == std::string_view::npos ? std::string_view{} : text.substr(begin, text.find_last_not_of(" \t\n\v\f\r") - begin + 1);
    const std::optional<value> read = text.find('.') == std::string_view::npos ? integer_spelling(number) : real_spelling(number);
    if(!read || read->kind() != value_kind::string)
        return read;
    const std::string spelling = lowered(text);
    if(spelling == "true" || spelling == "false")
        return value{spelling == "true"};
    return value{std::string(text)};
}

void bind_reference_names(eval_scope &over)
{
    over.set("true", value{true});
    over.set("True", value{true});
    over.set("false", value{false});
    over.set("False", value{false});
    over.set("e", *value::make_real(2.718281828459045));
    over.set("tau", *value::make_real(6.283185307179586));
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
    const std::optional<value> read = converted(*text);
    if(!read)
        return quoted + ", whose number " + *text + " is outside the range this evaluator carries";
    over.set(name, *read);
    return std::nullopt;
}

}

// Upstream evaluates the command's body over its argument table, with no property visible.
std::optional<std::string> eval_command(subst_ctx &ctx, std::string_view body)
{
    eval_scope over;
    bind_reference_names(over);
    for(const token &one : tokenize(body))
    {
        const std::optional<std::string> refused = one.kind == token_kind::name ? bind_read(ctx.scope, over, one.text) : std::nullopt;
        if(!refused)
            continue;
        ctx.log.log(level::error, diagnostic_code::expression_error, ctx.at, *refused);
        record_terminal(ctx, diagnostic_code::expression_error, *refused);
        return std::nullopt;
    }
    return eval_expr(ctx, body, over);
}

}
