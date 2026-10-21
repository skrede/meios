#ifndef HPP_GUARD_MEIOS_XACRO_SUBSTITUTION_LEX_H
#define HPP_GUARD_MEIOS_XACRO_SUBSTITUTION_LEX_H

#include "substitution_detail.h"

#include <string>
#include <cstddef>
#include <string_view>

namespace meios::detail
{

void refine_span_column(subst_ctx &ctx, std::size_t decoded_offset);

bool refuse_terminal(subst_ctx &ctx, diagnostic_code code, const std::string &message);

bool fail_unterminated(subst_ctx &ctx, char opener, std::size_t at);

// The reference's lexer reads one token ahead, so a token after a span that does not lex refuses before the span is evaluated.
bool refuse_follower(subst_ctx &ctx, std::string_view raw, std::size_t at, std::size_t base);

// Upstream's lexer token ^\$\$+(\{|\() escapes the opener; a run before anything else is no token.
bool expand_dollar_run(subst_ctx &ctx, std::string_view raw, std::size_t dollar, std::string &out,
                       std::size_t &cursor, std::size_t base);

}

#endif
