#ifndef HPP_GUARD_MEIOS_CLI_DEFERRED_ERROR_SINK_H
#define HPP_GUARD_MEIOS_CLI_DEFERRED_ERROR_SINK_H

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"
#include "meios/diagnostic/capturing_log_sink.h"

#include <string>
#include <vector>
#include <cstddef>

namespace meios::cli
{

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

    std::size_t errors() const
    {
        return m_errors.size();
    }

    void replay_first(log_sink &out) const
    {
        if(m_errors.empty())
            return;
        const captured_diagnostic &first = m_errors.front();
        out.log(level::error, first.code, first.loc, first.message);
    }

private:
    log_sink &m_inner;
    std::vector<captured_diagnostic> m_errors;
};

}

#endif
