#ifndef HPP_GUARD_MEIOS_XACRO_DEFERRED_MESSAGE_H
#define HPP_GUARD_MEIOS_XACRO_DEFERRED_MESSAGE_H

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/expansion_error.h"
#include "meios/diagnostic/source_location.h"
#include "meios/diagnostic/operation_failure.h"

#include <string>
#include <optional>
#include <string_view>

namespace meios::detail
{

// The sink one deferred property's evaluation logs to. Every record but an error passes straight
// to the reader, and the first error is held back so the level can append where it was defined
// and read before handing it on, which is how upstream chains "when evaluating" once per level.
class deferred_level_sink final : public log_sink
{
    struct held
    {
        std::optional<diagnostic_code> code;
        std::optional<source_location> where;
        std::string message;
        std::optional<operation_failure> cause;
    };

public:
    using log_sink::log;

    explicit deferred_level_sink(log_sink &reader);

    void log(level lvl, const std::string &message) override;
    void log(level lvl, const source_location &where, const std::string &message) override;
    void log(level lvl, diagnostic_code code, const source_location &where, const std::string &message) override;
    void log(level lvl, diagnostic_code code, const source_location &where, const operation_failure &cause, const std::string &message) override;

    // Hands the held refusal on with this level's clause, answering it as a cause when it carries a
    // code and a location; nothing was held when a deeper layer already reported elsewhere.
    std::optional<expansion_error> report(std::string_view name, const source_location &defined, const std::optional<source_location> &read) const;

private:
    log_sink &m_reader;
    std::optional<held> m_refusal;

    void hold(held refusal);
};

}

#endif
