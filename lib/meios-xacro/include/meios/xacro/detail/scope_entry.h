#ifndef HPP_GUARD_MEIOS_XACRO_SCOPE_ENTRY_H
#define HPP_GUARD_MEIOS_XACRO_SCOPE_ENTRY_H

#include "meios/xacro/value.h"
#include "meios/xacro/eval_failure_kind.h"

#include <string>
#include <cstddef>
#include <cstdint>
#include <variant>
#include <optional>

namespace meios
{

namespace detail
{

// A property's text as written, evaluated at its first read. The origin indexes a record the
// expansion that wrote it keeps, which is why the entry also carries that expansion's generation.
struct deferred_text
{
    std::string text;
    std::size_t origin;
    std::uint64_t generation;
};

// A deferred text whose evaluation refused. Its record is already logged, so a later read answers
// the kind without evaluating, or reporting, again.
struct failed_read
{
    eval_failure_kind kind;
};

using scope_entry = std::variant<value, deferred_text, failed_read>;

// A read's three answers: a value, absence (nothing bound, failure none), or a refusal of kind
// failure whose record a deferred evaluation already logged.
struct name_read
{
    std::optional<value> bound;
    eval_failure_kind failure;

    bool failed() const
    {
        return failure != eval_failure_kind::none;
    }
};

}

}

#endif
