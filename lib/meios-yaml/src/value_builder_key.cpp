#include "value_builder.h"
#include "scalar_resolver.h"

#include "meios/xacro/value.h"
#include "meios/xacro/value_render.h"

#include "meios/xacro/detail/value_key.h"

#include <yaml-cpp/mark.h>

#include <string>
#include <utility>
#include <optional>
#include <algorithm>

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

// The duplicate search runs over the key type's own equality, the same predicate the mapping
// constructor searches with, so this half and that one cannot disagree about whether a
// document repeats a key.
void value_builder::admit_key(scalar_key key, const YAML::Mark &mark)
{
    frame &top = m_frames.back();
    if(std::ranges::find(top.seen, key) != top.seen.end())
        throw stopped("a duplicate key '" + render_scalar(key) + '\'', mark,
                      yaml_failure::refused);
    top.seen.push_back(key);
    top.key = std::move(key);
}

void value_builder::take_key(const std::string &text, const std::string &tag,
                             const YAML::Mark &mark)
{
    const value made = resolved(text, tag, mark);
    const std::optional<std::string> name = made.text();
    if(name && *name == "<<")
        throw stopped("a merge key", mark, yaml_failure::unsupported);
    admit_key(key_of(made), mark);
}

}
