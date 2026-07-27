#include "urdf_detail.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <string>
#include <cstddef>
#include <string_view>

namespace meios::detail
{

namespace
{

bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

std::string_view next_token(std::string_view &rest)
{
    while(!rest.empty() && is_space(rest.front()))
        rest.remove_prefix(1);
    std::size_t end = 0;
    while(end < rest.size() && !is_space(rest[end]))
        ++end;
    const std::string_view token = rest.substr(0, end);
    rest.remove_prefix(end);
    return token;
}

std::size_t count_tokens(std::string_view text)
{
    std::size_t found = 0;
    while(!next_token(text).empty())
        ++found;
    return found;
}

bool parse_tokens(std::string_view text, double *out, std::size_t count, parse_context &ctx,
                  const source_location &loc, std::string_view field)
{
    bool ok = true;
    for(std::size_t i = 0; i < count; ++i)
        if(!read_finite(next_token(text), out[i], ctx, loc, field))
            ok = false;
    return ok;
}

// The message names counts only. The attribute's own text is author-controlled and
// would reach a consumer's log verbatim.
std::string arity_message(std::string_view field, std::size_t expected, std::size_t found)
{
    return "'" + std::string(field) + "' needs " + std::to_string(expected) + " components but has "
         + std::to_string(found);
}

}

bool read_scalars(std::string_view text, double *out, std::size_t count, parse_context &ctx,
                  const source_location &loc, std::string_view field)
{
    const std::size_t found = count_tokens(text);
    if(found == 0)
        return true;
    if(found != count)
    {
        ctx.log.log(level::error, diagnostic_code::vector_arity, loc,
                    arity_message(field, count, found));
        return false;
    }
    return parse_tokens(text, out, count, ctx, loc, field);
}

}
