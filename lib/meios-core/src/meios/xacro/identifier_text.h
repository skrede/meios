#ifndef HPP_GUARD_MEIOS_XACRO_IDENTIFIER_TEXT_H
#define HPP_GUARD_MEIOS_XACRO_IDENTIFIER_TEXT_H

#include <string>
#include <cstddef>
#include <optional>
#include <string_view>

namespace meios::detail
{

// The spelling Python stores for an identifier, which is its NFKC form, or nothing when the text
// is not an identifier.
std::optional<std::string> python_name(std::string_view spelling);

// The reference's is_valid_name: the name is a Python identifier, its own NFKC form, and no hard
// keyword.
bool valid_python_name(std::string_view name);

// CPython's potential identifier: an ASCII letter or underscore, or any byte outside ASCII, opens one
// and an ASCII digit may continue it.
bool opens_identifier(char byte);

std::size_t identifier_run(std::string_view source, std::size_t at);

// The name an expression reads for a run outside ASCII, or nothing when Python refuses to read it,
// including a run that normalizes to one of its constants.
std::optional<std::string> read_spelling(std::string_view run);

// CPython's lexical message for a run it refuses to read, or nothing for a run it reads and for text
// that opens no run. Malformed UTF-8, which the reference's XML parser never hands it, is named by
// its first bad byte.
std::optional<std::string> identifier_fault(std::string_view run);

}

#endif
