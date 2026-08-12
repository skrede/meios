#ifndef HPP_GUARD_MEIOS_XACRO_QUOTE_SCAN_H
#define HPP_GUARD_MEIOS_XACRO_QUOTE_SCAN_H

namespace meios::detail
{

// A backslash is not an escape here: a backslash-bearing literal is a spelling the
// expression lexer already refuses by name, so honoring the escape would only move
// which layer says no.
inline char step_quote(char open, char c)
{
    if(open != '\0')
        return c == open ? '\0' : open;
    if(c == '\'' || c == '"')
        return c;
    return open;
}

}

#endif
