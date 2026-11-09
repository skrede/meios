#ifndef HPP_GUARD_MEIOS_DETAIL_ARTICLE_H
#define HPP_GUARD_MEIOS_DETAIL_ARTICLE_H

#include <string>
#include <string_view>

namespace meios::detail
{

inline std::string with_article(std::string_view noun)
{
    const bool vowel = !noun.empty() && std::string_view("aeiou").find(noun.front()) != std::string_view::npos;
    return (vowel ? "an " : "a ") + std::string(noun);
}

}

#endif
