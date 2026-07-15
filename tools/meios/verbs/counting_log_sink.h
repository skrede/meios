#ifndef HPP_GUARD_MEIOS_CLI_COUNTING_LOG_SINK_H
#define HPP_GUARD_MEIOS_CLI_COUNTING_LOG_SINK_H

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/source_location.h"

#include <string>
#include <cstddef>

namespace meios::cli
{

// Forwards every record to an inner sink while counting the error-level ones, so a
// verb can gate its exit code and output on whether a load logged any error.
class counting_log_sink final : public log_sink
{
public:
    explicit counting_log_sink(log_sink &inner) : m_inner(inner), m_errors(0) {}

    void log(level lvl, const std::string &message) override
    {
        if(lvl == level::error)
            ++m_errors;
        m_inner.log(lvl, message);
    }

    void log(level lvl, const source_location &location, const std::string &message) override
    {
        if(lvl == level::error)
            ++m_errors;
        m_inner.log(lvl, location, message);
    }

    std::size_t errors() const
    {
        return m_errors;
    }

private:
    log_sink &m_inner;
    std::size_t m_errors;
};

}

#endif
