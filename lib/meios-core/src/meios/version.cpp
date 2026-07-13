#include <pugixml.hpp>

namespace meios::detail
{

// Anchors a real pugixml link edge in the core archive so the hidden PRIVATE
// dependency, and its exported $<LINK_ONLY:> reference, is exercised by a genuine
// symbol rather than shipped untested.
bool xml_engine_ready() noexcept
{
    pugi::xml_document document;
    return document.first_child().empty();
}

}
