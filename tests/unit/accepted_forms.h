#ifndef HPP_GUARD_MEIOS_UNIT_ACCEPTED_FORMS_H
#define HPP_GUARD_MEIOS_UNIT_ACCEPTED_FORMS_H

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string_view>

namespace accepted
{

// What this evaluator accepts, read off the sites where it dispatches: the atoms, one member per
// precedence level's operator, the membership kinds, the subscript and member reads, the string
// meanings, the mapping constructor, the mathematics called by bare name, the auxiliary-document
// load, and what such a document carries into an expression. A member is a spelling or a shape
// the evaluator decides on, never a coercion between kinds.
enum class form : std::uint32_t
{
    integer_literal,
    real_literal,
    string_literal,
    true_literal,
    false_literal,
    parenthesized_group,
    name_reference,
    pi_constant,
    unary_minus,
    unary_plus,
    logical_not,
    addition,
    subtraction,
    multiplication,
    true_division,
    floor_division,
    modulo,
    power,
    less_than,
    less_or_equal,
    greater_than,
    greater_or_equal,
    numeric_equality,
    numeric_inequality,
    chained_comparison,
    string_equality,
    logical_and,
    logical_or,
    conditional,
    mapping_membership,
    string_membership,
    mapping_subscript,
    sequence_subscript,
    negative_index,
    dotted_member,
    string_concatenation,
    string_truth,
    named_split,
    mapping_literal,
    auxiliary_document_load,
    absolute_value,
    arc_cosine,
    arc_sine,
    arc_tangent,
    arc_tangent_of_two,
    ceiling,
    cosine,
    to_degrees,
    floor,
    maximum,
    minimum,
    to_radians,
    sine,
    square_root,
    tangent,
    radians_tag,
    degrees_tag,
    meters_tag,
    millimeters_tag,
    foot_tag,
    inches_tag,
    document_sequence,
    alias,
    merge_key,
    duplicate_key,
    non_string_key,
};

struct form_spelling
{
    form named;
    std::string_view name;
};

// Closed at these spellings with no default arm, the way the construct vocabulary is closed: a
// spelling outside the table resolves to nothing rather than reaching a member by accident.
inline constexpr form_spelling form_spellings[] = {
    { form::integer_literal, "integer-literal" },
    { form::real_literal, "real-literal" },
    { form::string_literal, "string-literal" },
    { form::true_literal, "true-literal" },
    { form::false_literal, "false-literal" },
    { form::parenthesized_group, "parenthesized-group" },
    { form::name_reference, "name-reference" },
    { form::pi_constant, "pi-constant" },
    { form::unary_minus, "unary-minus" },
    { form::unary_plus, "unary-plus" },
    { form::logical_not, "logical-not" },
    { form::addition, "addition" },
    { form::subtraction, "subtraction" },
    { form::multiplication, "multiplication" },
    { form::true_division, "true-division" },
    { form::floor_division, "floor-division" },
    { form::modulo, "modulo" },
    { form::power, "power" },
    { form::less_than, "less-than" },
    { form::less_or_equal, "less-or-equal" },
    { form::greater_than, "greater-than" },
    { form::greater_or_equal, "greater-or-equal" },
    { form::numeric_equality, "numeric-equality" },
    { form::numeric_inequality, "numeric-inequality" },
    { form::chained_comparison, "chained-comparison" },
    { form::string_equality, "string-equality" },
    { form::logical_and, "logical-and" },
    { form::logical_or, "logical-or" },
    { form::conditional, "conditional" },
    { form::mapping_membership, "mapping-membership" },
    { form::string_membership, "string-membership" },
    { form::mapping_subscript, "mapping-subscript" },
    { form::sequence_subscript, "sequence-subscript" },
    { form::negative_index, "negative-index" },
    { form::dotted_member, "dotted-member" },
    { form::string_concatenation, "string-concatenation" },
    { form::string_truth, "string-truth" },
    { form::named_split, "named-split" },
    { form::mapping_literal, "mapping-literal" },
    { form::auxiliary_document_load, "auxiliary-document-load" },
    { form::absolute_value, "absolute-value" },
    { form::arc_cosine, "arc-cosine" },
    { form::arc_sine, "arc-sine" },
    { form::arc_tangent, "arc-tangent" },
    { form::arc_tangent_of_two, "arc-tangent-of-two" },
    { form::ceiling, "ceiling" },
    { form::cosine, "cosine" },
    { form::to_degrees, "to-degrees" },
    { form::floor, "floor" },
    { form::maximum, "maximum" },
    { form::minimum, "minimum" },
    { form::to_radians, "to-radians" },
    { form::sine, "sine" },
    { form::square_root, "square-root" },
    { form::tangent, "tangent" },
    { form::radians_tag, "radians-tag" },
    { form::degrees_tag, "degrees-tag" },
    { form::meters_tag, "meters-tag" },
    { form::millimeters_tag, "millimeters-tag" },
    { form::foot_tag, "foot-tag" },
    { form::inches_tag, "inches-tag" },
    { form::document_sequence, "document-sequence" },
    { form::alias, "alias" },
    { form::merge_key, "merge-key" },
    { form::duplicate_key, "duplicate-key" },
    { form::non_string_key, "non-string-key" }
};

inline constexpr std::size_t form_count = std::size(form_spellings);

// Each row sits at the index of the member it names, which makes the pairing total.
constexpr bool form_spellings_are_paired()
{
    for(std::size_t at = 0; at < form_count; ++at)
        if(form_spellings[at].named != static_cast<form>(at))
            return false;
    return true;
}

static_assert(form_spellings_are_paired(), "each spelling sits at the index of the form it names");
static_assert(static_cast<std::size_t>(form::non_string_key) + 1 == form_count,
              "every accepted form carries a spelling");

inline std::string_view form_name(form one)
{
    return form_spellings[static_cast<std::size_t>(one)].name;
}

inline std::optional<form> form_from_name(std::string_view spelling)
{
    for(const form_spelling &one : form_spellings)
    {
        if(one.name == spelling)
            return one.named;
    }
    return std::nullopt;
}

}

#endif
