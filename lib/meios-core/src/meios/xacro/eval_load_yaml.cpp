#include "lexer.h"
#include "eval_parser.h"
#include "eval_session.h"

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/value_render.h"
#include "meios/xacro/core_evaluator.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <string>
#include <optional>
#include <string_view>

namespace meios::detail
{
namespace
{

// eval_scope::parse_yaml has already reported its own cause — the missing capability, the
// construct this backend declines, the malformed document or the crossed ceiling — so the
// category is recorded here without a second diagnostic. An absent capability and a declined
// construct are both softenable, since a fuller backend reads either; the other two are not.
eval_failure_kind category_of(yaml_failure why)
{
    switch(why)
    {
        case yaml_failure::unavailable: return eval_failure_kind::unsupported;
        case yaml_failure::unsupported: return eval_failure_kind::unsupported;
        case yaml_failure::exhausted:   return eval_failure_kind::exhausted;
        default:                        return eval_failure_kind::error;
    }
}

value refuse_parsed(parser &p, yaml_failure why)
{
    if(p.ok)
        p.failure = category_of(why);
    p.ok = false;
    return value{};
}

value load_yaml_value(parser &p, std::string_view argument)
{
    const std::optional<value> bound = p.scope.lookup(argument);
    if(!bound)
        return p.fail("name '" + std::string(argument) + "' is not defined",
                      diagnostic_code::undefined_property);
    const std::optional<std::string> spec = bound->text();
    if(!spec)
        return p.fail("an auxiliary document is named by text, not by a "
                      + std::string(kind_name(bound->kind())));
    const std::optional<std::string> bytes = p.scope.load_text(*spec);
    if(!bytes)
        return p.fail("cannot reach the auxiliary document \"" + *spec + '"',
                      diagnostic_code::unresolved_asset);
    const yaml_outcome parsed =
        p.scope.parse_yaml(*bytes, p.session.limits, p.session.counters, p.log, p.anchor);
    if(!parsed.parsed)
        return refuse_parsed(p, parsed.failure);
    return *parsed.parsed;
}

}

value parse_load_yaml(parser &p)
{
    if(!p.accept(token_kind::lparen))
        return p.fail("expected '(' after xacro.load_yaml");
    if(!p.at(token_kind::name))
        return p.fail_unsupported("xacro.load_yaml takes one named document — use eval-python");
    const std::string_view argument = p.peek().text;
    ++p.pos;
    if(!p.accept(token_kind::rparen))
        return p.fail_unsupported("xacro.load_yaml takes one named document — use eval-python");
    return load_yaml_value(p, argument);
}

}
