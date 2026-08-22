#ifndef HPP_GUARD_MEIOS_UNIT_ACCEPTED_FORMS_COVERAGE_H
#define HPP_GUARD_MEIOS_UNIT_ACCEPTED_FORMS_COVERAGE_H

#include "accepted_forms.h"

#include <cstddef>
#include <iterator>
#include <optional>
#include <string_view>

namespace accepted
{

// What measures each accepted form: the record, and the keys in it whose rows drive that form.
// A form no committed row can measure carries the reason instead, and a row carrying neither is
// an empty cell rather than a claim, which the assertion below refuses.
struct coverage
{
    form named;
    std::string_view record;
    std::string_view keys;
    std::string_view reason;
};

// Paired by index against the vocabulary, and paired both ways against the records themselves by
// the stem beside this header: a form here naming no row fails, and a row no form names fails.
inline constexpr coverage form_coverage[] = {
    { form::integer_literal, "expressions", "n01", "" },
    { form::real_literal, "expressions", "n02", "" },
    { form::string_literal, "expressions", "n03", "" },
    { form::true_literal, "expressions", "n04", "" },
    { form::false_literal, "expressions", "n05", "" },
    { form::parenthesized_group, "expressions", "e16 n08", "" },
    { form::name_reference, "expressions", "n06", "" },
    { form::pi_constant, "expressions", "n07", "" },
    { form::unary_minus, "expressions", "e14 n09", "" },
    { form::unary_plus, "expressions", "n10 n11", "" },
    { form::logical_not, "expressions", "t13 t14", "" },
    { form::addition, "expressions", "e01 e03 e05 e07 e09 e12 n12", "" },
    { form::subtraction, "expressions", "e02 e04 e06 e08 e10 e13 n13 t08", "" },
    { form::multiplication, "expressions", "e17 n14", "" },
    { form::true_division, "expressions", "e15 n15 n36 t09", "" },
    { form::floor_division, "expressions", "n16 n17 n18 t10", "" },
    { form::modulo, "expressions", "n19 n20 n21 t11", "" },
    { form::power, "expressions", "n22 n23 t12", "" },
    { form::less_than, "expressions", "n24", "" },
    { form::less_or_equal, "expressions", "n25", "" },
    { form::greater_than, "expressions", "n26", "" },
    { form::greater_or_equal, "expressions", "n27", "" },
    { form::numeric_equality, "expressions", "n28", "" },
    { form::numeric_inequality, "expressions", "n29", "" },
    { form::chained_comparison, "expressions", "n30 n31", "" },
    { form::string_equality, "expressions", "e11", "" },
    { form::logical_and, "expressions", "n32 n33", "" },
    { form::logical_or, "expressions", "n34 n35", "" },
    { form::conditional, "expressions", "t15 t16 t17", "" },
    { form::mapping_membership, "expressions", "e18 d07", "" },
    { form::string_membership, "expressions", "t18 t19 t20 t21", "" },
    { form::mapping_subscript, "expressions", "e19 c01 c02 c03", "" },
    { form::sequence_subscript, "expressions", "s01 s03 s05", "" },
    { form::negative_index, "expressions", "s02 s04", "" },
    { form::dotted_member, "expressions", "p01 p02 p03", "" },
    { form::string_concatenation, "expressions", "t01 t02 t03 t04 t05 t06 t07", "" },
    { form::string_truth, "expressions", "t13 t14", "" },
    { form::named_split, "expressions", "t22 t23 t24 t25 t26 t27 t28 t29 t30 t31 t32 t33", "" },
    { form::mapping_literal, "expressions", "d01 d02 d03 d04 d05 d06 d07 d08 d09 d10", "" },
    { form::auxiliary_document_load, "expressions", "e20 e21 e22 e23", "" },
    { form::absolute_value, "", "",
      "upstream's expression globals carry the mathematics module and the two reducers and no "
      "builtins at all, so the absolute value refuses there and renders here; there is no "
      "upstream rendering to measure and the difference is held by the reviewed divergence "
      "div_absolute_value instead" },
    { form::arc_cosine, "expressions", "m01", "" },
    { form::arc_sine, "expressions", "m02", "" },
    { form::arc_tangent, "expressions", "m03", "" },
    { form::arc_tangent_of_two, "expressions", "m04", "" },
    { form::ceiling, "expressions", "m05", "" },
    { form::cosine, "expressions", "m06", "" },
    { form::to_degrees, "expressions", "m07", "" },
    { form::floor, "expressions", "m08", "" },
    { form::maximum, "expressions", "m09", "" },
    { form::minimum, "expressions", "m10", "" },
    { form::to_radians, "expressions", "m11", "" },
    { form::sine, "expressions", "m12", "" },
    { form::square_root, "expressions", "m13", "" },
    { form::tangent, "expressions", "m14", "" },
    { form::radians_tag, "unit_tags", "!radians", "" },
    { form::degrees_tag, "unit_tags", "!degrees", "" },
    { form::meters_tag, "unit_tags", "!meters", "" },
    { form::millimeters_tag, "unit_tags", "!millimeters", "" },
    { form::foot_tag, "unit_tags", "!foot", "" },
    { form::inches_tag, "unit_tags", "!inches", "" },
    { form::document_sequence, "", "",
      "selected by the auxiliary document's own bytes and never by an expression -- the same "
      "load reads a document with or without a sequence at its root -- so no committed record "
      "measures it and the native_yaml stems assert it at the reader's own tier" },
    { form::alias, "", "",
      "selected by the document's bytes and never by an expression, as the sequence-rooted "
      "document is; asserted at the reader's own tier by the native_yaml_alias stem" },
    { form::merge_key, "", "",
      "selected by the document's bytes and never by an expression, as the sequence-rooted "
      "document is; asserted at the reader's own tier by the native_yaml_merge stem" },
    { form::duplicate_key, "", "",
      "selected by the document's bytes and never by an expression, as the sequence-rooted "
      "document is; asserted at the reader's own tier by the native_yaml stem" },
    { form::non_string_key, "", "",
      "admitted by the reader and reachable by no subscript, every subscript key here being "
      "text, so no expression spelling drives it; held by the reviewed divergence "
      "non_text_mapping_key" }
};

inline constexpr std::size_t form_coverage_count = std::size(form_coverage);

constexpr bool form_coverage_is_paired()
{
    for(std::size_t at = 0; at < form_coverage_count; ++at)
        if(form_coverage[at].named != static_cast<form>(at))
            return false;
    return true;
}

// A form states either what measures it or why nothing can. Both filled is two answers, neither
// filled is none, and neither is a claim about coverage.
constexpr bool form_coverage_is_answered()
{
    for(const coverage &one : form_coverage)
        if(one.keys.empty() == one.reason.empty())
            return false;
    return true;
}

static_assert(form_coverage_is_paired(), "each row sits at the index of the form it covers");
static_assert(form_coverage_count == form_count, "every accepted form carries a coverage row");
static_assert(form_coverage_is_answered(), "each form names its rows or the reason it has none");

inline const coverage &coverage_of(form one)
{
    return form_coverage[static_cast<std::size_t>(one)];
}

}

#endif
