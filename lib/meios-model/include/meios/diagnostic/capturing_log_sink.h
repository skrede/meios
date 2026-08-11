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
#include <utility>
#include <optional>
#include <algorithm>

namespace meios
{

// Forwards every record to an inner sink and retains all of them in arrival order, at every
// level, separately tracking which of them were errors. A query takes a mark — a record count
// taken earlier — and answers only for what arrived after it, so one sink can serve several
// loads without a later one inheriting an earlier one's errors.
// A caller that builds its own sources constructs them against this sink and hands the same
// object to load(): a source reports through the sink it was constructed with, so this is the
// only channel by which a source's own failure reaches the load and refuses it.
// That reuse must be sequential: both lists are appended to without synchronization, so two
// loads running at once against one sink race.
class capturing_log_sink final : public log_sink
{
public:
    explicit capturing_log_sink(log_sink &inner)
            : m_inner(inner)
            , m_errors()
            , m_records()
    {
    }

    void log(level lvl, const std::string &message) override
    {
        capture(lvl, {diagnostic_code::unspecified, source_location{}, message});
        m_inner.log(lvl, message);
    }

    void log(level lvl, const source_location &location, const std::string &message) override
    {
        capture(lvl, {diagnostic_code::unspecified, location, message});
        m_inner.log(lvl, location, message);
    }

    void log(level lvl, diagnostic_code code, const source_location &location, const std::string &message) override
    {
        capture(lvl, {code, location, message});
        m_inner.log(lvl, code, location, message);
    }

    void log(level lvl, diagnostic_code code, const source_location &location, const operation_failure &cause, const std::string &message) override
    {
        capture(lvl, {code, location, message, cause});
        m_inner.log(lvl, code, location, cause, message);
    }

    std::size_t size() const
    {
        return m_records.size();
    }

    std::size_t errors(std::size_t mark = 0) const
    {
        std::size_t total = 0;
        for(std::size_t at : m_errors)
            if(at >= mark)
                ++total;
        return total;
    }

    std::optional<captured_diagnostic> first(std::size_t mark = 0) const
    {
        for(std::size_t at : m_errors)
            if(at >= mark)
                return m_records[at];
        return std::nullopt;
    }

    // A mark is a bare count carrying no tie to the sink it was taken from, so a mark taken
    // elsewhere reaches here as a plausible number; nothing above this line can rule it out.
    std::vector<captured_diagnostic> records(std::size_t mark = 0) const
    {
        // Parenthesized so a consumer that included <windows.h> first does not expand the min
        // macro here; this header is installed, so that translation unit is not ours to control.
        const std::ptrdiff_t from =
            static_cast<std::ptrdiff_t>((std::min)(mark, m_records.size()));
        return {m_records.begin() + from, m_records.end()};
    }

private:
    log_sink &m_inner;
    std::vector<std::size_t> m_errors;
    std::vector<captured_diagnostic> m_records;

    void capture(level lvl, captured_diagnostic record)
    {
        m_records.push_back(std::move(record));
        if(lvl == level::error)
            m_errors.push_back(m_records.size() - 1);
    }
};

// Everything a capturing_log_sink recorded after the moment the window was taken. A load
// measures its own outcome through one of these, because the sink it was handed may already
// have served an earlier load and may serve a later one.
class capture_window
{
public:
    explicit capture_window(capturing_log_sink &sink)
            : m_mark(sink.size())
            , m_sink(sink)
    {
    }

    log_sink &log() const noexcept
    {
        return m_sink;
    }

    std::size_t errors() const
    {
        return m_sink.errors(m_mark);
    }

    std::optional<captured_diagnostic> first() const
    {
        return m_sink.first(m_mark);
    }

    std::vector<captured_diagnostic> records() const
    {
        return m_sink.records(m_mark);
    }

private:
    std::size_t m_mark;
    capturing_log_sink &m_sink;
};

}

#endif
