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

// Anchors an unlocated diagnostic the injected backend emits at the hosting node, so one
// escaping a message-only evaluator still leaves the layer with a real position.
// The position is known here and the classification is not, so only an error is typed: a
// terminal failure has to carry a code for the latch beneath to return it as the structured
// cause, and reaching that latch is what an expression error means. A warning is anchored and
// left uncoded, because claims_from clears a completeness bit per record code without regard
// to level, so typing a warn would retract a claim on a load that succeeded.
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
        if(lvl == level::error)
            m_inner.log(lvl, diagnostic_code::expression_error, m_at, message);
        else if(lvl == level::warn)
            m_inner.log(lvl, m_at, message);
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
