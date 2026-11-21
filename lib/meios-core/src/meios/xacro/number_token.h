#ifndef HPP_GUARD_MEIOS_XACRO_NUMBER_TOKEN_H
#define HPP_GUARD_MEIOS_XACRO_NUMBER_TOKEN_H

#include "lexer.h"

#include <string>
#include <vector>
#include <cstddef>
#include <optional>
#include <string_view>

namespace meios::detail
{

// Reads the numeric literal starting at i and answers where reading resumes.
std::size_t push_number(std::string_view src, std::size_t i, std::vector<token> &out);

// The message of a numeric literal's error token, and nothing for any other text.
std::optional<std::string> number_fault(std::string_view text);

bool imaginary_literal(std::string_view text);

}

#endif
