#include "deferred_message.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/expansion_error.h"
#include "meios/diagnostic/source_location.h"
#include "meios/diagnostic/operation_failure.h"

#include <string>
#include <utility>
#include <optional>
#include <string_view>

namespace meios::detail
{

namespace
{

std::string place(const source_location &at)
{
    return at.file.generic_string() + ':' + std::to_string(at.line);
}

std::string level_clause(std::string_view name, const source_location &defined, const std::optional<source_location> &read)
{
    std::string clause = " when evaluating '" + std::string(name) + "' (defined at " + place(defined) + ')';
    if(read)
        clause += ", read at " + place(*read);
    return clause;
}

}

deferred_level_sink::deferred_level_sink(log_sink &reader)
        : m_reader(reader)
        , m_refusal()
{
}

void deferred_level_sink::log(level lvl, const std::string &message)
{
    if(lvl == level::error)
        hold(held{std::nullopt, std::nullopt, message, std::nullopt});
    else
        m_reader.log(lvl, message);
}

void deferred_level_sink::log(level lvl, const source_location &where, const std::string &message)
{
    if(lvl == level::error)
        hold(held{std::nullopt, where, message, std::nullopt});
    else
        m_reader.log(lvl, where, message);
}

void deferred_level_sink::log(level lvl, diagnostic_code code, const source_location &where, const std::string &message)
{
    if(lvl == level::error)
        hold(held{code, where, message, std::nullopt});
    else
        m_reader.log(lvl, code, where, message);
}

void deferred_level_sink::log(level lvl, diagnostic_code code, const source_location &where, const operation_failure &cause, const std::string &message)
{
    if(lvl == level::error)
        hold(held{code, where, message, cause});
    else
        m_reader.log(lvl, code, where, cause, message);
}

void deferred_level_sink::hold(held refusal)
{
    if(!m_refusal)
        m_refusal = std::move(refusal);
}

std::optional<expansion_error> deferred_level_sink::report(std::string_view name, const source_location &defined, const std::optional<source_location> &read) const
{
    if(!m_refusal)
        return std::nullopt;
    const held &refusal       = *m_refusal;
    const std::string message = refusal.message + level_clause(name, defined, read);
    if(!refusal.where)
        m_reader.log(level::error, message);
    else if(!refusal.code)
        m_reader.log(level::error, *refusal.where, message);
    else if(refusal.cause)
        m_reader.log(level::error, *refusal.code, *refusal.where, *refusal.cause, message);
    else
        m_reader.log(level::error, *refusal.code, *refusal.where, message);
    if(!refusal.where || !refusal.code)
        return std::nullopt;
    return expansion_error{*refusal.where, message, *refusal.code, refusal.cause};
}

}
