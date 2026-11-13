#ifndef HPP_GUARD_MEIOS_XACRO_XML_NAME_H
#define HPP_GUARD_MEIOS_XACRO_XML_NAME_H

#include <string_view>

namespace meios::detail
{

// Text that is malformed UTF-8 is not a name.
bool is_xml_name(std::string_view text);
// XML 1.0 (fifth edition), section 2.3, production [3] S, removed at either end only.
std::string_view trim_xml_space(std::string_view text);

}

#endif
