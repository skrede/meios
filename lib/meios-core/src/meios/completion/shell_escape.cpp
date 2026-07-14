#include "completion_detail.h"

namespace meios::detail
{

std::string shell_single_quote(std::string_view text)
{
    std::string out = "'";
    for(char c : text)
    {
        if(c == '\'')
            out += "'\\''";
        else
            out += c;
    }
    out += '\'';
    return out;
}

std::string long_option_name(std::string_view token)
{
    std::size_t start = token.find_first_not_of('-');
    if(start == std::string_view::npos)
        return std::string{ token };
    return std::string{ token.substr(start) };
}

std::string_view supported_shells()
{
    return "bash zsh fish";
}

}
