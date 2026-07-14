#include "substitution_detail.h"

#include "meios/xacro/substitution.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <string>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios
{

namespace
{

std::size_t find_close(std::string_view raw, std::size_t opener, char open_ch, char close_ch)
{
    int depth = 1;
    for(std::size_t k = opener + 1; k < raw.size(); ++k)
    {
        if(raw[k] == open_ch)
            ++depth;
        else if(raw[k] == close_ch && --depth == 0)
            return k;
    }
    return std::string_view::npos;
}

bool fail_unterminated(detail::subst_ctx &ctx, char opener, std::size_t at)
{
    ctx.log.log(level::error, std::string("unterminated $") + opener
        + " substitution span at offset " + std::to_string(at));
    return false;
}

bool expand_span(detail::subst_ctx &ctx, std::string_view raw, std::size_t dollar,
                 std::string &out, std::size_t &cursor)
{
    char opener = raw[dollar + 1];
    char closer = opener == '{' ? '}' : ')';
    std::size_t close = find_close(raw, dollar + 1, opener, closer);
    if(close == std::string_view::npos)
        return fail_unterminated(ctx, opener, dollar);
    std::string_view inner = raw.substr(dollar + 2, close - dollar - 2);
    std::optional<std::string> result =
        opener == '{' ? detail::eval_expr(ctx, inner) : detail::dispatch(ctx, inner);
    if(!result)
        return false;
    out.append(*result);
    cursor = close + 1;
    return true;
}

bool scan(detail::subst_ctx &ctx, std::string_view raw, std::string &out)
{
    std::size_t cursor = 0;
    while(cursor < raw.size())
    {
        std::size_t dollar = raw.find('$', cursor);
        if(dollar == std::string_view::npos || dollar + 1 >= raw.size())
            break;
        out.append(raw.substr(cursor, dollar - cursor));
        char opener = raw[dollar + 1];
        if(opener != '{' && opener != '(')
        {
            out.push_back('$');
            cursor = dollar + 1;
            continue;
        }
        if(!expand_span(ctx, raw, dollar, out, cursor))
            return false;
    }
    out.append(raw.substr(cursor));
    return true;
}

}

substitution substitute(std::string_view raw, const eval_scope &scope, source_stack &sources,
                        const std::filesystem::path &document, log_sink &log)
{
    detail::subst_ctx ctx(log, sources, scope, document);
    std::string out;
    bool ok = scan(ctx, raw, out);
    return substitution{ ok, std::move(out) };
}

}
