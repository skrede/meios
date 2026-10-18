#ifndef HPP_GUARD_MEIOS_UNIT_ERROR_SINK_H
#define HPP_GUARD_MEIOS_UNIT_ERROR_SINK_H

#include <meios/expected.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/expansion_error.h>
#include <meios/diagnostic/source_location.h>
#include <meios/diagnostic/operation_failure.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <optional>
#include <string_view>

namespace error_sink_test
{

struct error_record
{
    std::optional<meios::diagnostic_code> code;
    meios::source_location loc;
    std::string message;
};

// Every overload is overridden, so an error record is kept whichever shape carries it: an overload
// left to the base class would drop an uncoded record and let a count of one hide a second.
struct error_sink final : public meios::log_sink
{
    std::vector<error_record> errors;

    void log(meios::level lvl, const std::string &message) override
    {
        keep(lvl, std::nullopt, meios::source_location{}, message);
    }

    void log(meios::level lvl, const meios::source_location &loc, const std::string &message) override
    {
        keep(lvl, std::nullopt, loc, message);
    }

    void log(meios::level lvl, meios::diagnostic_code code, const meios::source_location &loc, const std::string &message) override
    {
        keep(lvl, code, loc, message);
    }

    void log(meios::level lvl, meios::diagnostic_code code, const meios::source_location &loc, const meios::operation_failure &, const std::string &message) override
    {
        keep(lvl, code, loc, message);
    }

    void keep(meios::level lvl, std::optional<meios::diagnostic_code> code, const meios::source_location &loc, const std::string &message)
    {
        if(lvl == meios::level::error)
            errors.push_back({code, loc, message});
    }
};

inline void one_record_at(const error_sink &heard, meios::diagnostic_code code, const meios::source_location &at)
{
    CHECK(heard.errors.size() == 1);
    for(const error_record &one : heard.errors)
    {
        INFO("recorded " << (one.code ? meios::to_string(*one.code) : "no code") << " at " << meios::to_string(one.loc) << ": " << one.message);
        CHECK(one.code == code);
        CHECK(one.loc.file == at.file);
        CHECK(one.loc.line == at.line);
    }
}

inline void every_record_names(const error_sink &heard, std::string_view fragment)
{
    for(const error_record &one : heard.errors)
    {
        INFO("recorded " << one.message);
        CHECK(one.message.find(fragment) != std::string::npos);
    }
}

template<typename T>
void ends_on_one_record(const meios::expected<T, meios::expansion_error> &out, const error_sink &heard, meios::diagnostic_code code, const meios::source_location &at)
{
    REQUIRE_FALSE(out.has_value());
    CHECK(out.error().code == code);
    CHECK(out.error().loc.file == at.file);
    CHECK(out.error().loc.line == at.line);
    one_record_at(heard, code, at);
}

}

#endif
