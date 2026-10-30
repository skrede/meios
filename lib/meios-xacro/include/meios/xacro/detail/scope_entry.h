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

// An include namespace bound under its name: the table it opened, valid only while the expansion
// that opened it runs, since that expansion's return drops the table.
struct namespace_entry
{
    std::size_t table;
    std::uint64_t generation;
};

// A block parameter bound under its starred name: an index into the blocks the expansion that bound
// it keeps, valid only while that expansion runs.
struct block_entry
{
    std::size_t block;
    std::uint64_t generation;
};

using scope_entry = std::variant<value, deferred_text, failed_read, namespace_entry, block_entry>;

// A read's four answers: a value, absence (nothing bound, failure none), a refusal of kind failure
// whose record a deferred evaluation already logged, or an include namespace, which has no value.
// A block has no value either, and reads as absent.
struct name_read
{
    std::optional<value> bound;
    eval_failure_kind failure;
    std::optional<namespace_entry> space;

    bool failed() const
    {
        return failure != eval_failure_kind::none;
    }
};

}

}

#endif
