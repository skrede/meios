#ifndef HPP_GUARD_MEIOS_XACRO_SUBSTITUTION_SINKS_H
#define HPP_GUARD_MEIOS_XACRO_SUBSTITUTION_SINKS_H

#include "substitution_detail.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/expansion_error.h"
#include "meios/diagnostic/source_location.h"
#include "meios/diagnostic/operation_failure.h"

#include <string>
#include <vector>
#include <optional>

namespace meios::detail
{

// Buffers an evaluator's diagnostics so the policy layer can decide their fate: a
// genuine error is replayed loud, an unsupported-under-leniency failure is dropped.
class capture_sink final : public log_sink
{
    // A record replays through the overload it arrived on: absence of a location or of a
    // code is what the evaluator said, and inventing either on replay would put a position
    // and a classification on a diagnostic that never carried one.
    struct buffered
    {
        level lvl;
        std::optional<diagnostic_code> code;
        std::optional<source_location> where;
        std::string message;
        std::optional<operation_failure> cause;
    };

public:
    using log_sink::log;

    void log(level lvl, const std::string &message) override
    {
        m_records.push_back({lvl, std::nullopt, std::nullopt, message, std::nullopt});
    }

    void log(level lvl, const source_location &where, const std::string &message) override
    {
        m_records.push_back({lvl, std::nullopt, where, message, std::nullopt});
    }

    void log(level lvl, diagnostic_code code, const source_location &where, const std::string &message) override
    {
        m_records.push_back({lvl, code, where, message, std::nullopt});
    }

    void log(level lvl, diagnostic_code code, const source_location &where, const operation_failure &cause, const std::string &message) override
    {
        m_records.push_back({lvl, code, where, message, cause});
    }

    void replay(log_sink &sink) const
    {
        for(const buffered &entry : m_records)
            replay_one(entry, sink);
    }

    void replay_warnings(log_sink &sink) const
    {
        for(const buffered &entry : m_records)
        {
            if(entry.lvl == level::warn)
                replay_one(entry, sink);
        }
    }

private:
    std::vector<buffered> m_records;

    static void replay_one(const buffered &entry, log_sink &sink)
    {
        if(!entry.where)
            sink.log(entry.lvl, entry.message);
        else if(!entry.code)
            sink.log(entry.lvl, *entry.where, entry.message);
        else if(entry.cause)
            sink.log(entry.lvl, *entry.code, *entry.where, *entry.cause, entry.message);
        else
            sink.log(entry.lvl, *entry.code, *entry.where, entry.message);
    }
};

// Upgrades an unlocated, code-less error or warning the injected backend emits to a typed,
// located record anchored at the hosting node, so a diagnostic escaping a message-only
// evaluator still leaves the layer with a code and a real position.
// The message-only overload asserts the code rather than receiving one, because a backend
// emitting through it has none to give: every unlocated code-less backend warn on this path is
// therefore typed as an expression error. That is right for every warn the path carries today,
// so a backend warn that is not about an expression must arrive through the overload carrying
// its own code rather than relying on this one.
class relocating_sink final : public log_sink
{
public:
    using log_sink::log;

    relocating_sink(log_sink &inner, const source_location &at)
            : m_inner(inner)
            , m_at(at)
    {
    }

    void log(level lvl, const std::string &message) override
    {
        if(lvl == level::error || lvl == level::warn)
            m_inner.log(lvl, diagnostic_code::expression_error, m_at, message);
        else
            m_inner.log(lvl, message);
    }

    void log(level lvl, const source_location &where, const std::string &message) override
    {
        m_inner.log(lvl, where, message);
    }

    void log(level lvl, diagnostic_code code, const source_location &where, const std::string &message) override
    {
        m_inner.log(lvl, code, where, message);
    }

    void log(level lvl, diagnostic_code code, const source_location &where, const operation_failure &cause, const std::string &message) override
    {
        m_inner.log(lvl, code, where, cause, message);
    }

private:
    log_sink &m_inner;
    source_location m_at;
};

// Forwards every record unchanged and keeps the first coded, located error as the
// substitution's structured cause. Which failure an evaluation hit — an undefined name,
// a rejected expression, a withheld capability — is known only to the layer that emitted
// the diagnostic, so a refusal reports that code rather than the generic one the calling
// site could name.
class latching_sink final : public log_sink
{
public:
    using log_sink::log;

    explicit latching_sink(subst_ctx &ctx)
            : m_ctx(ctx)
    {
    }

    void log(level lvl, const std::string &message) override
    {
        m_ctx.log.log(lvl, message);
    }

    void log(level lvl, const source_location &where, const std::string &message) override
    {
        m_ctx.log.log(lvl, where, message);
    }

    void log(level lvl, diagnostic_code code, const source_location &where, const std::string &message) override
    {
        latch(lvl, expansion_error{where, message, code, std::nullopt});
        m_ctx.log.log(lvl, code, where, message);
    }

    void log(level lvl, diagnostic_code code, const source_location &where, const operation_failure &cause, const std::string &message) override
    {
        latch(lvl, expansion_error{where, message, code, cause});
        m_ctx.log.log(lvl, code, where, cause, message);
    }

private:
    subst_ctx &m_ctx;

    void latch(level lvl, const expansion_error &candidate)
    {
        if(lvl == level::error)
            record_terminal(m_ctx, candidate);
    }
};

}

#endif
