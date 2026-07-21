#include "text_location.h"

#include <cstddef>
#include <algorithm>
#include <string_view>

namespace meios::detail
{

int offset_to_line(std::string_view text, std::ptrdiff_t offset)
{
    const std::ptrdiff_t stop = std::min<std::ptrdiff_t>(offset, static_cast<std::ptrdiff_t>(text.size()));
    int line = 1;
    for(std::ptrdiff_t i = 0; i < stop; ++i)
        if(text[static_cast<std::size_t>(i)] == '\n')
            ++line;
    return line;
}

source_location offset_location(std::string_view text, std::ptrdiff_t offset,
                                const std::filesystem::path &file)
{
    const std::ptrdiff_t stop = std::min<std::ptrdiff_t>(offset, static_cast<std::ptrdiff_t>(text.size()));
    int column = 1;
    for(std::ptrdiff_t i = 0; i < stop; ++i)
        column = text[static_cast<std::size_t>(i)] == '\n' ? 1 : column + 1;
    return source_location{ file, offset_to_line(text, offset), column };
}

source_location node_location(pugi::xml_node node, std::string_view text,
                              const std::filesystem::path &file)
{
    return offset_location(text, node.offset_debug(), file);
}

}
