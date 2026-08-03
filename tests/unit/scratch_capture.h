#ifndef HPP_GUARD_MEIOS_TEST_SCRATCH_CAPTURE_H
#define HPP_GUARD_MEIOS_TEST_SCRATCH_CAPTURE_H

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/source_location.h>
#include <meios/diagnostic/operation_failure.h>

#include <string>
#include <vector>
#include <optional>

namespace scratch_test
{

struct event
{
    meios::level           lvl;
    meios::diagnostic_code code;
    std::string            text;

    std::optional<meios::operation_failure> cause;
};

using event_log = std::vector<event>;

// The cause-carrying overload is the one that matters: without it the functor sink falls back
// to the four-argument log and a case asserting on a cause would pass vacuously.
struct capture
{
    event_log &events;

    void operator()(meios::level lvl, const std::string &message)
    {
        events.push_back({ lvl, meios::diagnostic_code::unspecified, message, std::nullopt });
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        events.push_back({ lvl, meios::diagnostic_code::unspecified, message, std::nullopt });
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &,
                    const std::string &message)
    {
        events.push_back({ lvl, code, message, std::nullopt });
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &,
                    const meios::operation_failure &cause, const std::string &message)
    {
        events.push_back({ lvl, code, message, cause });
    }
};

}

#endif
