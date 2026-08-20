#include "lexer.h"
#include "eval_parser.h"
#include "eval_numeric_ops.h"

#include "meios/xacro/value.h"
#include "meios/xacro/value_render.h"

#include <string>
#include <optional>

namespace meios::detail
{
namespace
{

bool is_comparison(token_kind k)
{
    return k == token_kind::less || k == token_kind::less_equal || k == token_kind::greater
        || k == token_kind::greater_equal || k == token_kind::equal_equal
        || k == token_kind::not_equal;
}

bool compare_numbers(parser &p, token_kind op, const value &a, const value &b)
{
    double x = need_double(p, a), y = need_double(p, b);
    switch(op)
    {
        case token_kind::less:          return x <  y;
        case token_kind::less_equal:    return x <= y;
        case token_kind::greater:       return x >  y;
        case token_kind::greater_equal: return x >= y;
        case token_kind::equal_equal:   return x == y;
        case token_kind::not_equal:     return x != y;
        default:                        return false;
    }
}

// A string compares only for equality, and only ever against another string's text: the
// closure measures no ordering between strings, and coercing one to a number is how a
// description quietly means something other than it says.
bool compare_text(parser &p, token_kind op, const value &a, const value &b)
{
    if(op != token_kind::equal_equal && op != token_kind::not_equal)
    {
        p.fail_unsupported("ordering comparison against a string — use eval-python");
        return false;
    }
    const bool same = a.kind() == b.kind() && a.text() == b.text();
    return op == token_kind::equal_equal ? same : !same;
}

bool compare_pair(parser &p, token_kind op, const value &a, const value &b)
{
    if(a.kind() == value_kind::string || b.kind() == value_kind::string)
        return compare_text(p, op, a, b);
    return compare_numbers(p, op, a, b);
}

// Containment is by bytes where upstream counts code points; the two agree over the ASCII text
// every measured description carries, and the empty needle is contained in every string.
value substring_of(parser &p, const value &needle, const std::string &haystack)
{
    const std::optional<std::string> text = needle.text();
    if(!text)
        return p.fail("only a string is contained in a string, not a "
                      + std::string(kind_name(needle.kind())));
    return value{ haystack.find(*text) != std::string::npos };
}

// Python chains a membership test and a relational operator as one comparison — `a in b == c`
// means `(a in b) and (b == c)` — where these sit at separate precedence levels here. That
// cannot silently mean the other thing: either spelling puts a boolean on one side of a
// membership test, and a boolean is neither a mapping key nor a substring, so a mixed run
// always refuses.
value contained_in(parser &p, const value &needle, const value &haystack)
{
    if(haystack.kind() == value_kind::string)
        return substring_of(p, needle, *haystack.text());
    if(haystack.kind() != value_kind::mapping)
        return p.fail_unsupported("membership against a "
                                  + std::string(kind_name(haystack.kind()))
                                  + " — use eval-python");
    const std::optional<std::string> name = needle.text();
    if(!name)
        return p.fail("a " + std::string(kind_name(needle.kind())) + " is not a key here");
    return value{ haystack.at(*name).has_value() };
}

}

value parse_comparison(parser &p)
{
    const level_guard level(p);
    if(!level.admitted())
        return value{};
    value left = parse_add(p);
    if(!is_comparison(p.peek().kind))
        return left;
    bool result = true;
    while(is_comparison(p.peek().kind))
    {
        token_kind op = p.peek().kind;
        ++p.pos;
        value right = parse_add(p);
        result = result && compare_pair(p, op, left, right);
        left = right;
    }
    return value{ result };
}

value parse_membership(parser &p)
{
    const level_guard level(p);
    if(!level.admitted())
        return value{};
    value left = parse_comparison(p);
    while(p.accept(token_kind::kw_in))
        left = contained_in(p, left, parse_comparison(p));
    return left;
}

}
