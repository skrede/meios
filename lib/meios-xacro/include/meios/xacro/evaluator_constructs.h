#ifndef HPP_GUARD_MEIOS_XACRO_EVALUATOR_CONSTRUCTS_H
#define HPP_GUARD_MEIOS_XACRO_EVALUATOR_CONSTRUCTS_H

#include <cstdint>
#include <cstddef>
#include <iterator>
#include <optional>
#include <string_view>

namespace meios
{

// What one load exercised, named by the two layers that can see it: the auxiliary-document
// builder marks the first six as it reads a document and the expression evaluator marks the
// last eight as it evaluates, each at the site where the construct is actually exercised.
// Nothing a third layer would have to be asked about is admitted, because a member nothing
// marks would state a claim about a load rather than observe one.
enum class evaluator_construct : std::uint32_t
{
    document_sequence,
    alias,
    merge_key,
    duplicate_key,
    non_string_key,
    unit_tag,
    dotted_member,
    sequence_subscript,
    negative_index,
    mapping_literal,
    string_concatenation,
    string_membership,
    string_truth,
    named_split,
};

struct construct_spelling
{
    evaluator_construct construct;
    std::string_view name;
};

// The vocabulary is closed at these spellings and there is no default arm, the way the unit-tag
// table refuses a tag it does not carry: a spelling outside the table resolves to nothing rather
// than reaching a member by accident.
inline constexpr construct_spelling construct_spellings[] = {
    { evaluator_construct::document_sequence, "document-sequence" },
    { evaluator_construct::alias, "alias" },
    { evaluator_construct::merge_key, "merge-key" },
    { evaluator_construct::duplicate_key, "duplicate-key" },
    { evaluator_construct::non_string_key, "non-string-key" },
    { evaluator_construct::unit_tag, "unit-tag" },
    { evaluator_construct::dotted_member, "dotted-member" },
    { evaluator_construct::sequence_subscript, "sequence-subscript" },
    { evaluator_construct::negative_index, "negative-index" },
    { evaluator_construct::mapping_literal, "mapping-literal" },
    { evaluator_construct::string_concatenation, "string-concatenation" },
    { evaluator_construct::string_membership, "string-membership" },
    { evaluator_construct::string_truth, "string-truth" },
    { evaluator_construct::named_split, "named-split" }
};

inline constexpr std::size_t evaluator_construct_count = std::size(construct_spellings);

// Each row sits at the index of the member it names, which is what lets a member read its own
// spelling without a search and makes the pairing total rather than merely intended.
constexpr bool construct_spellings_are_paired()
{
    for(std::size_t at = 0; at < evaluator_construct_count; ++at)
        if(construct_spellings[at].construct != static_cast<evaluator_construct>(at))
            return false;
    return true;
}

static_assert(construct_spellings_are_paired(),
              "each spelling sits at the index of the member it names");
static_assert(static_cast<std::size_t>(evaluator_construct::named_split) + 1
                  == evaluator_construct_count,
              "every member of the vocabulary carries a spelling");

inline std::string_view construct_name(evaluator_construct one)
{
    return construct_spellings[static_cast<std::size_t>(one)].name;
}

inline std::optional<evaluator_construct> construct_from_name(std::string_view spelling)
{
    for(const construct_spelling &one : construct_spellings)
    {
        if(one.name == spelling)
            return one.construct;
    }
    return std::nullopt;
}

// What a load exercised rather than how often it did: one bit per member keeps the observation
// a single word, trivially copyable and zero-initialized like every counter it sits beside.
class construct_set
{
public:
    construct_set() : m_bits(0) {}

    void mark(evaluator_construct one) { m_bits |= bit(one); }

    bool holds(evaluator_construct one) const { return (m_bits & bit(one)) != 0; }

    bool empty() const { return m_bits == 0; }

    bool operator==(const construct_set &other) const = default;

private:
    std::uint32_t m_bits;

    static std::uint32_t bit(evaluator_construct one)
    {
        return std::uint32_t{ 1 } << static_cast<std::uint32_t>(one);
    }
};

static_assert(evaluator_construct_count <= 32, "the vocabulary fits the set's word");

}

#endif
