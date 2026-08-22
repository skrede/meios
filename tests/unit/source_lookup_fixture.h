#ifndef HPP_GUARD_MEIOS_TEST_SOURCE_LOOKUP_FIXTURE_H
#define HPP_GUARD_MEIOS_TEST_SOURCE_LOOKUP_FIXTURE_H

#include <meios/io/scratch_dir.h>
#include <meios/io/source_lookup.h>
#include <meios/io/package_source.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/source_location.h>
#include <meios/diagnostic/operation_failure.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <utility>
#include <optional>
#include <algorithm>
#include <filesystem>
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

inline bool errored(const std::vector<record> &records)
{
    return std::any_of(records.begin(), records.end(), [](const record &noted) { return noted.level == meios::level::error; });
}

inline meios::scratch_dir make_tree()
{
    std::error_code error;
    const std::filesystem::path parent = std::filesystem::temp_directory_path(error);
    REQUIRE_FALSE(error);
    const meios::expected<std::filesystem::path, meios::operation_failure> root = meios::detail::create_scratch_root(parent);
    REQUIRE(root.has_value());
    return meios::scratch_dir{*root};
}

inline void seed(const std::filesystem::path &file)
{
    std::filesystem::create_directories(file.parent_path());
    std::ofstream(file) << "mesh-bytes";
}

inline std::filesystem::path real_path(const std::filesystem::path &candidate)
{
    std::error_code error;
    const std::filesystem::path real = std::filesystem::weakly_canonical(candidate, error);
    REQUIRE_FALSE(error);
    return real;
}

// A self-referential directory link: a real, deterministic object no supported platform can
// canonicalize, so a native traversal failure is injected without a scripted seam.
inline std::filesystem::path loop_root(const std::filesystem::path &under)
{
    std::error_code error;
    std::filesystem::create_directory_symlink(under / "loop", under / "loop", error);
    REQUIRE_FALSE(error);
    return under / "loop";
}

}

#endif
