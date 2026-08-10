#ifndef HPP_GUARD_MEIOS_YAML_CONTAINED_SINK_H
#define HPP_GUARD_MEIOS_YAML_CONTAINED_SINK_H

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"
#include "meios/diagnostic/operation_failure.h"

#include <string>

namespace meios::detail
{

// The sink a caller installs is code this module does not own, and it runs inside the
// third-party parser's own recursion. Wrapping it once at the module's entry keeps a sink that
// raises from turning a documented refusal into a throw across a returned-value contract; a
// record the sink itself refuses is lost, which is the only outcome left once the one channel
// for saying so is the thing that failed.
class contained_sink final : public log_sink
{
public:
    explicit contained_sink(log_sink &inner) : m_inner(inner) {}

    void log(level lvl, const std::string &message) override
    {
        guard([&] { m_inner.log(lvl, message); });
    }

    void log(level lvl, const source_location &where, const std::string &message) override
    {
        guard([&] { m_inner.log(lvl, where, message); });
    }

    void log(level lvl, diagnostic_code code, const source_location &where,
             const std::string &message) override
    {
        guard([&] { m_inner.log(lvl, code, where, message); });
    }

    void log(level lvl, diagnostic_code code, const source_location &where,
             const operation_failure &cause, const std::string &message) override
    {
        guard([&] { m_inner.log(lvl, code, where, cause, message); });
    }

private:
    log_sink &m_inner;

    template <typename Attempt>
    static void guard(Attempt attempt)
    {
        try
        {
            attempt();
        }
        catch(...)
        {
        }
    }
};

}

#endif
