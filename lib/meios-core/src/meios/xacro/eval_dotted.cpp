#include "lexer.h"
#include "eval_parser.h"
#include "eval_numeric_ops.h"

#include "meios/xacro/value.h"
#include "meios/xacro/eval_scope.h"
#include "meios/xacro/value_render.h"

#include <span>
#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <optional>
#include <algorithm>
#include <string_view>

namespace meios::detail
{
namespace
{

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

bool push_field(parser &p, std::vector<value> &fields, std::string text)
{
    if(!admits_element(p))
        return false;
    fields.push_back(text_result(p, std::move(text)));
    return p.ok;
}

// The separator form splits on every occurrence and keeps the field between two adjacent
// separators and at either end, so the field count is one more than the separator count. The
// no-argument form collapses runs of whitespace instead, which is a different algorithm.
value split_text(parser &p, const std::string &text, const std::string &separator)
{
    std::vector<value> fields;
    std::size_t at = 0;
    for(std::size_t found = text.find(separator); found != std::string::npos;
        found = text.find(separator, at))
    {
        if(!push_field(p, fields, text.substr(at, found - at)))
            return value{};
        at = found + separator.size();
    }
    if(!push_field(p, fields, text.substr(at)))
        return value{};
    p.exercised(evaluator_construct::named_split);
    return value::make_sequence(std::move(fields));
}

value parse_split(parser &p, const std::string &text)
{
    p.accept(token_kind::lparen);
    if(p.at(token_kind::rparen))
        return p.fail_unsupported("split with no separator collapses whitespace — write the "
                                  "separator, as split(' ') — use eval-python");
    const value argument = parse_ternary(p);
    if(!p.ok)
        return value{};
    if(!p.accept(token_kind::rparen))
        return p.fail_unsupported("split takes one separator here — use eval-python");
    const std::optional<std::string> separator = argument.text();
    if(!separator)
        return p.fail("the separator must be text, not a "
                      + std::string(kind_name(argument.kind())));
    if(separator->empty())
        return p.fail("an empty separator does not split a string");
    return split_text(p, text, *separator);
}

// Text, not privilege: the origin mark does not gate this, because upstream reads the same
// operation off a string whether a document produced it or an author wrote it. One operation is
// admitted by name, and no other member spelling on a string yields anything at all.
value string_member(parser &p, const std::string &text, std::string_view member)
{
    if(member == "split" && p.at(token_kind::lparen))
        return parse_split(p, text);
    return p.fail_unsupported("a string answers no member '" + std::string(member)
                              + "' here — use eval-python");
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
    if(base.kind() == value_kind::string)
        return string_member(p, *base.text(), member);
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
    p.exercised(evaluator_construct::dotted_member);
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
