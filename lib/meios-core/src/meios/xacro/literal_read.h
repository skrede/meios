#ifndef HPP_GUARD_MEIOS_XACRO_LITERAL_READ_H
#define HPP_GUARD_MEIOS_XACRO_LITERAL_READ_H

#include "meios/xacro/value.h"

#include <string_view>

namespace meios::detail
{

// xacro's literal reading of a property value (Table._eval_literal): a value wholly enclosed in
// single quotes loses them, a value holding '_' stays text, then int(), float() and the four
// boolean spellings are tried in that order. An integer outside 64 bits keeps its canonical digits
// as text, and a real that is not finite keeps the text as written.
value read_literal(std::string_view text);

// A bound string is read as a literal; a value of any other kind keeps its type.
value read_literal(const value &bound);

}

#endif
