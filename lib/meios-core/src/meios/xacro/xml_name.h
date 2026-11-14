#ifndef HPP_GUARD_MEIOS_XACRO_XML_NAME_H
#define HPP_GUARD_MEIOS_XACRO_XML_NAME_H

#include <string>
#include <string_view>

namespace meios::detail
{

// Text that is malformed UTF-8 is not a name.
bool is_xml_name(std::string_view text);
// XML 1.0 (fifth edition), section 2.3, production [3] S, removed at either end only.
std::string_view trim_xml_space(std::string_view text);
// A tab, carriage return or line feed is written \t, \r or \n and any other control byte as \x and two
// hex digits, so quoted text cannot split a one-record-per-line log.
std::string escaped_controls(std::string_view text);

}

#endif
