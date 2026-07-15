#ifndef HPP_GUARD_MEIOS_XACRO_CONTAINER_MARKER_H
#define HPP_GUARD_MEIOS_XACRO_CONTAINER_MARKER_H

namespace meios::detail
{

// Delimits a container (list/dict/tuple) str-ified across a xacro:property boundary so it can be
// round-tripped: the producer wraps it, rehydrate unwraps it. XML 1.0 forbids this control char
// (www.w3.org/TR/xml/#charsets), so it never occurs in a legitimate resolved value; any stray
// occurrence is stripped at every value->output seam and so never reaches serialized output.
inline constexpr char container_marker = '\x01';

}

#endif
