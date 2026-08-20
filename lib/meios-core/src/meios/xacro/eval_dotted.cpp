#include "lexer.h"
#include "eval_parser.h"
#include "eval_session.h"

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/value_render.h"
#include "meios/xacro/core_evaluator.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <span>
#include <string>
#include <optional>
#include <algorithm>
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

value refuse_dotted(parser &p)
{
    return p.fail_unsupported("dotted access in an expression — use eval-python");
}

// Measured into tests/golden/oracle/collisions.cases, and checked against it rather than
// asserted: upstream's mapping wrapper tries ordinary attribute lookup before falling through
// to the document, so each of these names answers with the wrapper's own operation and the key
// beneath it is never reached.
constexpr std::string_view colliding[] = { "clear",      "copy",   "fromkeys", "get",
                                           "items",      "keys",   "pop",      "popitem",
                                           "setdefault", "update", "values" };

bool collides(std::string_view member)
{
    return std::ranges::find(colliding, member) != std::ranges::end(colliding);
}

}

std::span<const std::string_view> colliding_members()
{
    return colliding;
}

// The origin mark is the whole gate: a mapping an author wrote never gains the member path a
// loaded one has, by whatever route it arrived. Above the gate sit the two spellings upstream
// answers itself — a name the wrapper carries, and any name written with a leading double
// underscore — which refuse rather than returning the key beneath them, because returning it
// would mean something here that it does not mean upstream on exactly the spelling written.
value member_of(parser &p, const value &base, std::string_view member)
{
    const level_guard level(p);
    if(!level.admitted())
        return value{};
    if(!base.from_yaml())
        return p.fail_unsupported("dotted access '." + std::string(member)
                                  + "' on a value that is not an auxiliary document"
                                    " — use eval-python");
    if(base.kind() != value_kind::mapping)
        return p.fail("a " + std::string(kind_name(base.kind())) + " has no member '"
                      + std::string(member) + '\'');
    if(member.starts_with("__"))
        return p.fail_unsupported("a member written with a leading '__' reaches the object "
                                  "protocol rather than the document — use eval-python");
    if(collides(member))
        return p.fail_unsupported("'" + std::string(member) + "' names a mapping operation "
                                  "upstream rather than a key — write it as ['"
                                  + std::string(member) + "'] — use eval-python");
    return index_into(p, base, value{ std::string(member) });
}

value parse_member(parser &p, const value &base)
{
    if(!p.at(token_kind::name))
        return refuse_dotted(p);
    const std::string_view member = p.peek().text;
    ++p.pos;
    return member_of(p, base, member);
}

// The one namespaced call the measured surface uses. A head naming nothing in scope is a
// namespace and refuses by name, because a description that means something else here than it
// does upstream is worse than one that will not load; a head naming a value reaches the same
// gate a dot after a subexpression reaches.
value parse_dotted(parser &p, std::string_view head)
{
    if(!p.at(token_kind::name))
        return refuse_dotted(p);
    const std::string_view member = p.peek().text;
    ++p.pos;
    if(head == "xacro" && member == "load_yaml")
        return parse_load_yaml(p);
    const std::optional<value> bound = p.scope.lookup(head);
    if(!bound)
        return p.fail_unsupported("unsupported call target '" + std::string(head) + '.'
                                  + std::string(member) + "' — use eval-python");
    return member_of(p, *bound, member);
}

}
