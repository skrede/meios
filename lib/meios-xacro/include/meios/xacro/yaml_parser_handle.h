#ifndef HPP_GUARD_MEIOS_XACRO_YAML_PARSER_HANDLE_H
#define HPP_GUARD_MEIOS_XACRO_YAML_PARSER_HANDLE_H

#include "meios/xacro/value.h"
#include "meios/xacro/evaluator_limits.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/source_location.h"

#include <memory>
#include <cassert>
#include <utility>
#include <optional>
#include <string_view>

namespace meios
{

// Why a parse produced no value. Only an absent capability and a construct a fuller backend
// would read are forms of unsupported syntax a lenient evaluation policy may soften; a
// malformed document and a crossed ceiling are terminal, and collapsing the four would let a
// hostile document leave partial output behind.
enum class yaml_failure
{
    none,
    unavailable,
    unsupported,
    refused,
    exhausted,
};

// The reason travels back with the result of the call that produced it, so a parser reachable
// from two concurrent loads keeps no failure state to read.
struct yaml_outcome
{
    std::optional<value> parsed;
    yaml_failure failure;
};

// Move-only type-erased hook turning auxiliary bytes into a value, so no evaluator ever
// resolves or opens a path itself: the bytes arrive from the scope's contained loader and
// the parser only reads them. The handle keeps no state of its own — the ceilings, the
// counters and the sink are per-load and arrive per call — so one parser is safe to share
// across concurrent loads. It names no parsing library, only core and value types.
class yaml_parser_handle
{
public:
    struct parser
    {
        parser() = default;
        parser(const parser &) = default;
        parser &operator=(const parser &) = default;
        parser(parser &&) = default;
        parser &operator=(parser &&) = default;
        virtual ~parser() = default;

        virtual yaml_outcome parse(std::string_view bytes, const evaluator_limits &limits,
                                   evaluator_counters &counters, log_sink &log,
                                   const source_location &at) const = 0;
    };

    yaml_parser_handle() = default;
    explicit yaml_parser_handle(std::unique_ptr<parser> impl) : m_impl(std::move(impl)) {}

    yaml_parser_handle(yaml_parser_handle &&) noexcept = default;
    yaml_parser_handle &operator=(yaml_parser_handle &&) noexcept = default;
    yaml_parser_handle(const yaml_parser_handle &) = delete;
    yaml_parser_handle &operator=(const yaml_parser_handle &) = delete;

    ~yaml_parser_handle() = default;

    bool valid() const noexcept { return m_impl != nullptr; }

    yaml_outcome operator()(std::string_view bytes, const evaluator_limits &limits,
                            evaluator_counters &counters, log_sink &log,
                            const source_location &at) const
    {
        assert(valid() && "parse on a moved-from yaml_parser_handle");
        return m_impl->parse(bytes, limits, counters, log, at);
    }

private:
    std::unique_ptr<parser> m_impl;
};

}

#endif
