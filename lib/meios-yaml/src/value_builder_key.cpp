#include "value_builder.h"

#include "meios/xacro/value.h"

#include "meios/xacro/detail/value_key.h"

#include <yaml-cpp/mark.h>
#include <yaml-cpp/anchor.h>

#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <optional>

namespace meios::detail
{
namespace
{

// The scalar resolver yields no collection, so text is the only kind left once the four
// others are ruled out.
scalar_key key_of(const value &one)
{
    switch(one.kind())
    {
        case value_kind::null:    return scalar_key{};
        case value_kind::boolean: return scalar_key{ *one.boolean() };
        case value_kind::integer: return scalar_key{ *one.integer() };
        case value_kind::real:    return scalar_key{ *one.real() };
        default:                  return scalar_key{ *one.text() };
    }
}

}

void value_builder::admit_key(scalar_key key)
{
    if(key.kind() != value_kind::string)
        exercised(evaluator_construct::non_string_key);
    m_frames.back().key = std::move(key);
}

// Only a plain spelling of the merge key merges; the quoted one is an ordinary text key, which
// is the one place the non-specific tag still separates two identical-looking scalars.
void value_builder::take_key(const std::string &text, const std::string &tag,
                             YAML::anchor_t anchor, const YAML::Mark &mark)
{
    const value made = resolved(text, tag, mark);
    const std::optional<std::string> name = made.text();
    remember(anchor, made.with_yaml_origin(), 1);
    if(tag == "?" && name && *name == "<<")
    {
        exercised(evaluator_construct::merge_key);
        m_frames.back().merging = true;
        return;
    }
    admit_key(key_of(made));
}

// A collection is unrepresentable as a key, so an alias standing where a key belongs is the
// same refusal a written collection there would be.
void value_builder::share(const anchored &source, const YAML::Mark &mark)
{
    if(!expecting_key())
        return deliver(source.held, source.denoted, YAML::NullAnchor, mark);
    if(source.held.kind() == value_kind::sequence)
        throw stopped("a sequence as a mapping key", mark, yaml_failure::refused);
    if(source.held.kind() == value_kind::mapping)
        throw stopped("a mapping as a mapping key", mark, yaml_failure::refused);
    admit_key(key_of(source.held));
}

// A merge source that is a sequence contributes its elements in reverse, so an earlier element
// wins a conflict; that is what the reference loader's own flattening does.
void value_builder::merge_into(frame &top, const value &source, const YAML::Mark &mark)
{
    top.merging = false;
    if(source.kind() != value_kind::sequence)
        return gather(top.merged, source, mark);
    for(std::size_t at = source.size(); at > 0; --at)
        gather(top.merged, *source.at(at - 1), mark);
}

void value_builder::gather(std::vector<value::entry> &into, const value &source,
                           const YAML::Mark &mark)
{
    if(source.kind() != value_kind::mapping)
        throw stopped("a merge of a value that is not a mapping", mark, yaml_failure::refused);
    for(std::size_t at = 0; at < source.size(); ++at)
        into.emplace_back(*source.key_at(at), *source.at(at));
}

}
