#ifndef HPP_GUARD_MEIOS_CLI_LABEL_ESCAPE_H
#define HPP_GUARD_MEIOS_CLI_LABEL_ESCAPE_H

#include <string>
#include <string_view>

namespace meios::cli
{

// Escapes an untrusted name for a Graphviz double-quoted label: the two structural
// characters plus the control bytes a hostile link name could use to forge DOT
// structure or a terminal escape sequence.
std::string escape_dot(std::string_view name);

// Neutralizes control bytes in an untrusted name before it reaches the terminal so
// a crafted name cannot inject an escape sequence into the ASCII rendering.
std::string escape_ascii(std::string_view name);

}

#endif
