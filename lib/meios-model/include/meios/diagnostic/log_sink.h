#ifndef HPP_GUARD_MEIOS_MODEL_DIAGNOSTIC_LOG_SINK_H
#define HPP_GUARD_MEIOS_MODEL_DIAGNOSTIC_LOG_SINK_H

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"
#include "meios/diagnostic/operation_failure.h"

#include <string>
#include <ostream>
#include <utility>
#include <type_traits>

namespace meios
{

class log_sink
{
public:
    log_sink()          = default;
    virtual ~log_sink() = default;

    virtual void log(level lvl, const std::string &message)
    {
        (void)lvl;
        (void)message;
    }

    virtual void log(level lvl, const source_location &location, const std::string &message)
    {
        (void)lvl;
        (void)location;
        (void)message;
    }

    virtual void log(level lvl, diagnostic_code code, const source_location &location, const std::string &message)
    {
        (void)code;
        log(lvl, location, message);
    }

    virtual void log(level lvl, diagnostic_code code, const source_location &location, const operation_failure &cause, const std::string &message)
    {
        (void)cause;
        log(lvl, code, location, message);
    }

protected:
    log_sink(const log_sink &)            = default;
    log_sink &operator=(const log_sink &) = default;
    log_sink(log_sink &&)                 = default;
    log_sink &operator=(log_sink &&)      = default;
};

template<typename Callable>
class log_sink_f final : public log_sink
{
public:
    explicit log_sink_f(Callable callable)
            : m_callable(std::move(callable))
    {
    }

    void log(level lvl, const std::string &message) override
    {
        m_callable(lvl, message);
    }

    void log(level lvl, const source_location &location, const std::string &message) override
    {
        m_callable(lvl, location, message);
    }

    void log(level lvl, diagnostic_code code, const source_location &location, const std::string &message) override
    {
        if constexpr(std::is_invocable_v<Callable, level, diagnostic_code, const source_location &, const std::string &>)
            m_callable(lvl, code, location, message);
        else
            m_callable(lvl, location, message);
    }

    void log(level lvl, diagnostic_code code, const source_location &location, const operation_failure &cause, const std::string &message) override
    {
        if constexpr(std::is_invocable_v<Callable, level, diagnostic_code, const source_location &, const operation_failure &, const std::string &>)
            m_callable(lvl, code, location, cause, message);
        else
            log(lvl, code, location, message);
    }

private:
    Callable m_callable;
};

template<typename Callable>
log_sink_f(Callable) -> log_sink_f<Callable>;

class log_sink_s final : public log_sink
{
public:
    explicit log_sink_s(std::ostream &stream)
            : m_stream(stream)
    {
    }

    void log(level lvl, const std::string &message) override
    {
        m_stream << '[' << to_string(lvl) << "] " << message << '\n';
    }

    void log(level lvl, const source_location &location, const std::string &message) override
    {
        m_stream << to_string(location) << ": [" << to_string(lvl) << "] " << message << '\n';
    }

    void log(level lvl, diagnostic_code code, const source_location &location, const std::string &message) override
    {
        m_stream << to_string(location) << ": [" << to_string(lvl) << "] (" << to_string(code) << ") " << message << '\n';
    }

    void log(level lvl, diagnostic_code code, const source_location &location, const operation_failure &, const std::string &message) override
    {
        log(lvl, code, location, message);
    }

private:
    std::ostream &m_stream;
};

}

#endif
