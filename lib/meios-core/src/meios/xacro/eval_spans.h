#ifndef HPP_GUARD_MEIOS_XACRO_EVAL_SPANS_H
#define HPP_GUARD_MEIOS_XACRO_EVAL_SPANS_H

#include "lexer.h"

#include <vector>
#include <cstddef>

namespace meios::detail
{

// Where a conditional begins, where a group ends and where a boolean operand ends, answered
// for every position by one right-to-left pass. A precedence level that scanned forward at its
// own entry would answer the same question quadratically — parse_ternary alone is re-entered
// from five sites, so a nested expression would rescan the same tokens once per level.
struct token_spans
{
    struct row
    {
        std::size_t conditional;
        std::size_t group;
        std::size_t and_operand;
        std::size_t or_operand;
    };

    // Answers with the position itself when no conditional governs it, which is the emptiness
    // test parse_ternary makes before it moves the cursor at all.
    std::size_t conditional_at(std::size_t at) const { return rows[at].conditional; }
    std::size_t group_end(std::size_t at) const { return rows[at].group; }
    std::size_t and_operand_end(std::size_t at) const { return rows[at].and_operand; }
    std::size_t or_operand_end(std::size_t at) const { return rows[at].or_operand; }

    std::vector<row> rows;
};

token_spans build_token_spans(const std::vector<token> &tokens);

}

#endif
