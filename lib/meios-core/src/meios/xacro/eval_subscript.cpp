#include "eval_parser.h"

#include "meios/xacro/value.h"
#include "meios/xacro/value_render.h"

#include "meios/diagnostic/diagnostic_code.h"

#include <string>
#include <cstdint>
#include <cstddef>
#include <optional>

namespace meios::detail
{
namespace
{

// The container's own contents never reach a message: a description author is told which key or
// which index was asked for, not what the auxiliary document holds.
value key_into(parser &p, const value &container, const value &key)
{
    const std::optional<std::string> name = key.text();
    if(!name)
        return p.fail("a " + std::string(kind_name(key.kind())) + " is not a subscript key here");
    const std::optional<value> found = container.at(*name);
    if(!found)
        return p.fail("key '" + *name + "' is not present", diagnostic_code::undefined_property);
    return *found;
}

// A negative index counts back from the end, so -1 is the last element. A real is refused rather
// than truncated to a position: upstream reads it as a type error and an index this evaluator
// invented would mean something there that it does not mean here.
value position_into(parser &p, const value &container, const value &key)
{
    if(!is_int(key))
        return p.fail("a sequence index must be an integer, not a "
                      + std::string(kind_name(key.kind())));
    const std::int64_t asked = *as_int(key);
    const std::int64_t extent = static_cast<std::int64_t>(container.size());
    const std::int64_t at = asked < 0 ? asked + extent : asked;
    if(at < 0 || at >= extent)
        return p.fail("index " + std::to_string(asked) + " is outside the sequence",
                      diagnostic_code::undefined_property);
    p.exercised(evaluator_construct::sequence_subscript);
    if(asked < 0)
        p.exercised(evaluator_construct::negative_index);
    return *container.at(static_cast<std::size_t>(at));
}

}

value index_into(parser &p, const value &container, const value &key)
{
    if(container.kind() == value_kind::mapping)
        return key_into(p, container, key);
    if(container.kind() == value_kind::sequence)
        return position_into(p, container, key);
    return p.fail("a " + std::string(kind_name(container.kind()))
                  + " cannot be subscripted here");
}

}
