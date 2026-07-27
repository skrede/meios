#ifndef HPP_GUARD_MEIOS_MODEL_DIAGNOSTIC_CAPTURING_LOG_SINK_H
#define HPP_GUARD_MEIOS_MODEL_DIAGNOSTIC_CAPTURING_LOG_SINK_H

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"
#include "meios/diagnostic/captured_diagnostic.h"

#include <string>
#include <vector>
#include <cstddef>
#include <optional>

namespace meios
{

// Forwards every record to an inner sink and retains all of them in arrival order, at every
// level, while separately counting the error-level ones and keeping the first error's full
// (code, loc, message). drive_load reads the count to decide success vs. failure and relays
// the captured diagnostic as the load_error, so a specific error can leave load() without
// being threaded through the pipeline.
class capturing_log_sink final : public log_sink
{
public:
    explicit capturing_log_sink(log_sink &inner) : m_inner(inner) {}

    void log(level lvl, const std::string &message) override
    {
        m_records.push_back(
            captured_diagnostic{ diagnostic_code::unspecified, source_location{}, message });
        if(lvl == level::error)
        {
            ++m_errors;
            if(!m_first)
                m_first = m_records.back();
        }
        m_inner.log(lvl, message);
    }

    void log(level lvl, const source_location &location, const std::string &message) override
    {
        m_records.push_back(
            captured_diagnostic{ diagnostic_code::unspecified, location, message });
        if(lvl == level::error)
        {
            ++m_errors;
            if(!m_first)
                m_first = m_records.back();
        }
        m_inner.log(lvl, location, message);
    }

    void log(level lvl, diagnostic_code code, const source_location &location, const std::string &message) override
    {
        m_records.push_back(captured_diagnostic{ code, location, message });
        if(lvl == level::error)
        {
            ++m_errors;
            if(!m_first)
                m_first = m_records.back();
        }
        m_inner.log(lvl, code, location, message);
    }

    std::size_t errors() const
    {
        return m_errors;
    }

    const std::optional<captured_diagnostic> &first() const
    {
        return m_first;
    }

    const std::vector<captured_diagnostic> &records() const
    {
        return m_records;
    }

private:
    log_sink &m_inner;
    std::size_t m_errors = 0;
    std::optional<captured_diagnostic> m_first;
    std::vector<captured_diagnostic> m_records;
};

}

#endif
