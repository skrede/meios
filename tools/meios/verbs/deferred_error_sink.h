#ifndef HPP_GUARD_MEIOS_CLI_DEFERRED_ERROR_SINK_H
#define HPP_GUARD_MEIOS_CLI_DEFERRED_ERROR_SINK_H

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/load_error.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"
#include "meios/diagnostic/operation_failure.h"
#include "meios/diagnostic/capturing_log_sink.h"

#include <string>
#include <vector>
#include <cstddef>

namespace meios::cli
{

// A record that arrived with a native cause leaves with it; one that never carried a cause
// goes out through the coded overload rather than gaining a fabricated one.
inline void relay_record(const captured_diagnostic &record, log_sink &out)
{
    if(record.cause)
        out.log(level::error, record.code, record.loc, *record.cause, record.message);
    else
        out.log(level::error, record.code, record.loc, record.message);
}

// The other half of the deferral below: a verb relays the load's own primary failure and
// never its diagnostic vector, which is audit data for the caller rather than a print list.
inline void relay_failure(const load_error &error, log_sink &out)
{
    relay_record({error.code, error.loc, error.message, error.cause}, out);
}

// Withholds error-level records from the inner sink, buffering them so a verb that
// relays a load's typed error at the top level prints that failure exactly once;
// warn/info records still forward live. replay_first re-emits the buffered first
// error for the source-bound rejection class that never reaches load()'s relay.
class deferred_error_sink final : public log_sink
{
public:
    using log_sink::log;

    explicit deferred_error_sink(log_sink &inner)
            : m_inner(inner)
            , m_errors()
    {
    }

    void log(level lvl, const std::string &message) override
    {
        if(lvl == level::error)
        {
            m_errors.push_back({diagnostic_code::unspecified, source_location{}, message});
            return;
        }
        m_inner.log(lvl, message);
    }

    void log(level lvl, const source_location &location, const std::string &message) override
    {
        if(lvl == level::error)
        {
            m_errors.push_back({diagnostic_code::unspecified, location, message});
            return;
        }
        m_inner.log(lvl, location, message);
    }

    void log(level lvl, diagnostic_code code, const source_location &location, const std::string &message) override
    {
        if(lvl == level::error)
        {
            m_errors.push_back({code, location, message});
            return;
        }
        m_inner.log(lvl, code, location, message);
    }

    void log(level lvl, diagnostic_code code, const source_location &location, const operation_failure &cause, const std::string &message) override
    {
        if(lvl == level::error)
        {
            m_errors.push_back({code, location, message, cause});
            return;
        }
        m_inner.log(lvl, code, location, cause, message);
    }

    std::size_t errors() const
    {
        return m_errors.size();
    }

    void replay_first(log_sink &out) const
    {
        if(m_errors.empty())
            return;
        relay_record(m_errors.front(), out);
    }

private:
    log_sink &m_inner;
    std::vector<captured_diagnostic> m_errors;
};

}

#endif
