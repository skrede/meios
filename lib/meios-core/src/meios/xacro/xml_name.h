#ifndef HPP_GUARD_MEIOS_XACRO_XML_NAME_H
#define HPP_GUARD_MEIOS_XACRO_XML_NAME_H

#include <string_view>

namespace meios::detail
{

// Text that is malformed UTF-8 is not a name.
bool is_xml_name(std::string_view text);

}

#endif
