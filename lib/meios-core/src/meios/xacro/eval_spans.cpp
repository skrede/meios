#include "lexer.h"
#include "eval_spans.h"

#include <limits>
#include <vector>
#include <cstddef>

namespace meios::detail
{
namespace
{

constexpr std::size_t unset = std::numeric_limits<std::size_t>::max();

struct running
{
    std::size_t conditional;
    std::size_t group;
    std::size_t and_operand;
    std::size_t or_operand;
};

// A delimiter is its own answer for the three span fields, but not for the conditional one:
// there the absence of a governing 'if' is spelled by the row's own index, so a group holding
// no conditional reports emptiness at every position instead of naming its delimiter.
running restarted(std::size_t at)
{
    return running{ unset, at, at, at };
}

bool opens_group(token_kind kind)
{
    return kind == token_kind::lparen || kind == token_kind::lbracket;
}

bool closes_group(token_kind kind)
{
    return kind == token_kind::rparen || kind == token_kind::rbracket;
}

bool cuts_group(token_kind kind)
{
    return kind == token_kind::comma || kind == token_kind::end;
}

void stop_operands(running &state, std::size_t at)
{
    state.and_operand = at;
    state.or_operand = at;
}

// An operand of 'or' may legitimately contain an 'and', so 'and' stops only its own level.
void apply_keyword(running &state, token_kind kind, std::size_t at)
{
    if(kind == token_kind::kw_and) { state.and_operand = at; return; }
    if(kind == token_kind::kw_or)  { stop_operands(state, at); return; }
    if(kind != token_kind::kw_if && kind != token_kind::kw_else) return;
    stop_operands(state, at);
    state.conditional = kind == token_kind::kw_if ? at : unset;
}

void step(running &state, std::vector<running> &saved, token_kind kind, std::size_t at)
{
    if(closes_group(kind)) { saved.push_back(state); state = restarted(at); return; }
    if(cuts_group(kind))   { state = restarted(at); return; }
    if(!opens_group(kind)) { apply_keyword(state, kind, at); return; }
    if(saved.empty()) return;
    state = saved.back();
    saved.pop_back();
}

token_spans::row written(const running &state, std::size_t at)
{
    return token_spans::row{ state.conditional == unset ? at : state.conditional, state.group,
                             state.and_operand, state.or_operand };
}

}

token_spans build_token_spans(const std::vector<token> &tokens)
{
    token_spans spans;
    spans.rows.resize(tokens.size());
    running state = restarted(0);
    std::vector<running> saved;
    for(std::size_t at = tokens.size(); at > 0;)
    {
        --at;
        step(state, saved, tokens[at].kind, at);
        spans.rows[at] = written(state, at);
    }
    return spans;
}

}
