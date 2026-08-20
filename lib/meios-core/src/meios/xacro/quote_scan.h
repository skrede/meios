#ifndef HPP_GUARD_MEIOS_XACRO_QUOTE_SCAN_H
#define HPP_GUARD_MEIOS_XACRO_QUOTE_SCAN_H

#include <cstddef>
#include <string_view>

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

// A brace inside a string literal is that literal's text and so steers no span. The scanner
// and the exact-span predicate must agree on where a span ends, so both ask this.
inline std::size_t find_expr_close(std::string_view raw, std::size_t opener)
{
    char open = '\0';
    int depth = 1;
    for(std::size_t k = opener + 1; k < raw.size(); ++k)
    {
        open = step_quote(open, raw[k]);
        if(open != '\0')
            continue;
        if(raw[k] == '{')
            ++depth;
        else if(raw[k] == '}' && --depth == 0)
            return k;
    }
    return std::string_view::npos;
}

}

#endif
