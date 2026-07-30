#ifndef HPP_GUARD_MEIOS_TEST_SOURCE_LOOKUP_FIXTURE_H
#define HPP_GUARD_MEIOS_TEST_SOURCE_LOOKUP_FIXTURE_H

#include <meios/io/source_lookup.h>
#include <meios/io/package_source.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/source_location.h>
#include <meios/diagnostic/operation_failure.h>

#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <string_view>
#include <system_error>

namespace acquisition_test
{

class lookup_category final : public std::error_category
{
public:
    const char *name() const noexcept override
    {
        return "lookup-test";
    }

    std::string message(int value) const override
    {
        return std::to_string(value);
    }
};

inline const lookup_category lookup_error_category;

enum class answer
{
    absent,
    found,
    failed,
};

struct typed_source
{
    meios::capability_descriptor capabilities() const
    {
        return descriptor;
    }

    std::optional<meios::resolved_asset> locate(std::string_view package, std::string_view relative) const
    {
        meios::source_lookup_result hit = try_locate(package, relative);
        return hit ? std::move(*hit) : std::nullopt;
    }

    meios::source_lookup_result try_locate(std::string_view, std::string_view) const
    {
        if(result == answer::found)
            return std::optional{meios::resolved_asset{path}};
        if(result == answer::failed)
            return meios::unexpected<meios::operation_failure>({meios::operation_kind::canonicalize, {error, lookup_error_category}});
        return std::optional<meios::resolved_asset>{};
    }

    answer result;
    std::string path;
    int error;
    meios::capability_descriptor descriptor{meios::source_kind::memory, false};
};

struct legacy_source
{
    meios::capability_descriptor capabilities() const
    {
        return descriptor;
    }

    std::optional<meios::resolved_asset> locate(std::string_view, std::string_view) const
    {
        return path.empty() ? std::nullopt : std::optional{meios::resolved_asset{path}};
    }

    std::string path;
    meios::capability_descriptor descriptor{meios::source_kind::memory, false};
};

struct record
{
    meios::level level;
    meios::diagnostic_code code;
    std::optional<meios::operation_failure> cause;
};

struct recorder
{
    void operator()(meios::level level, const std::string &)
    {
        records.push_back({level, meios::diagnostic_code::unspecified, std::nullopt});
    }

    void operator()(meios::level level, const meios::source_location &, const std::string &)
    {
        records.push_back({level, meios::diagnostic_code::unspecified, std::nullopt});
    }

    void operator()(meios::level level, meios::diagnostic_code code, const meios::source_location &, const std::string &)
    {
        records.push_back({level, code, std::nullopt});
    }

    void operator()(meios::level level, meios::diagnostic_code code, const meios::source_location &, const meios::operation_failure &cause, const std::string &)
    {
        records.push_back({level, code, cause});
    }

    std::vector<record> &records;
};

}

#endif
