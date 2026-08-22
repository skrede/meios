#ifndef HPP_GUARD_MEIOS_XACRO_CONTAINER_MARKER_H
#define HPP_GUARD_MEIOS_XACRO_CONTAINER_MARKER_H

#include <array>

namespace meios::detail
{

// Delimits a container (list/dict/tuple) str-ified across a xacro:property boundary so it can be
// round-tripped, the delimiter's identity also carrying whether the container came from a yaml
// load. XML 1.0 forbids both of these control chars (www.w3.org/TR/xml/#charsets), so neither
// occurs in a legitimate resolved value; every member of the set is stripped at every
// value->output seam and so none reaches serialized output. \x00 cannot join the set: the emit
// path ends in set_value(text.c_str()), which would truncate there.
inline constexpr char plain_container_marker = '\x01';
inline constexpr char yaml_container_marker  = '\x02';

inline constexpr std::array<char, 2> container_markers{ plain_container_marker,
                                                        yaml_container_marker };

}

#endif
