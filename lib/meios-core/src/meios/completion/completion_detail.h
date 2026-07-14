#ifndef HPP_GUARD_MEIOS_COMPLETION_COMPLETION_DETAIL_H
#define HPP_GUARD_MEIOS_COMPLETION_COMPLETION_DETAIL_H

#include <string>
#include <string_view>

namespace meios::detail
{

// POSIX single-quote wrapping: the only metacharacter left live inside single
// quotes is the single quote itself, closed and re-opened as '\''. Candidate and
// description text emitted into a shell script must pass through this so it stays
// inert data, never a place for a metacharacter to break out.
std::string shell_single_quote(std::string_view text);

// The long-option name a shell wants, with the leading dashes of the table token
// stripped (`--package-path` -> `package-path`).
std::string long_option_name(std::string_view token);

// The shells with an emitter; also the completion verb's fixed argument set.
std::string_view supported_shells();

}

#endif
