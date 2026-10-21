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

// Upstream's command pattern is ^\$\([^\)]*\): a command ends at its first closing parenthesis
// whatever it holds, quotes included, so a call or a span written inside one truncates it there.
inline std::size_t find_cmd_close(std::string_view raw, std::size_t opener)
{
    return raw.find(')', opener + 1);
}

// Upstream's tokens, tried in order: ^\$\$+(\{|\(), ^\$\{[^\}]*\}, ^\$\([^\)]*\), [^$]+|\$[^{($]+|\$$.
// Only this lookahead token ends at its first brace; a span's own close is still found quote-aware.
inline bool lexes_at(std::string_view raw, std::size_t at)
{
    if(at + 1 >= raw.size() || raw[at] != '$')
        return true;
    const char next = raw[at + 1];
    if(next == '$')
    {
        const std::size_t end = raw.find_first_not_of('$', at);
        return end != std::string_view::npos && (raw[end] == '{' || raw[end] == '(');
    }
    if(next == '{')
        return raw.find('}', at + 2) != std::string_view::npos;
    if(next == '(')
        return find_cmd_close(raw, at + 1) != std::string_view::npos;
    return true;
}

}

#endif
