#include "lexer.h"
#include "eval_parser.h"

#include "meios/xacro/value.h"

#include <string>

namespace meios::detail
{

value parse_postfix(parser &p)
{
    const level_guard level(p);
    if(!level.admitted())
        return value{};
    value base = parse_atom(p);
    while(p.ok)
    {
        if(p.accept(token_kind::dot))
        {
            base = parse_member(p, base);
            continue;
        }
        if(!p.accept(token_kind::lbracket))
            break;
        const value key = parse_ternary(p);
        if(p.ok && p.at(token_kind::unsupported))
            return p.fail_unsupported("unsupported subscript form at '"
                                      + std::string(p.peek().text) + "' — use eval-python");
        if(!p.accept(token_kind::rbracket))
            return p.fail("expected ']' after a subscript key");
        base = index_into(p, base, key);
    }
    return base;
}

}
