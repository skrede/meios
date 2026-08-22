#ifndef HPP_GUARD_MEIOS_UNIT_ASSET_READ_PROBE_H
#define HPP_GUARD_MEIOS_UNIT_ASSET_READ_PROBE_H

// A scanner stem needs a sink that keeps what it is given rather than counting it, and a read
// failure it can create without privileges: a directory behaves the same way on every platform.

#include <meios/io/scratch_dir.h>
#include <meios/io/resolved_asset.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/operation_failure.h>

#include <catch2/catch_test_macros.hpp>

#include <random>
#include <string>
#include <vector>
#include <fstream>
#include <optional>
#include <algorithm>
#include <filesystem>

namespace asset_probe
{

struct captured
{
    meios::level lvl;
    meios::diagnostic_code code;
    std::optional<meios::operation_failure> cause;
};

struct recorder
{
    std::vector<captured> &sink;

    void operator()(meios::level lvl, const std::string &) { sink.push_back({ lvl, meios::diagnostic_code::unspecified, std::nullopt }); }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &) { sink.push_back({ lvl, meios::diagnostic_code::unspecified, std::nullopt }); }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &, const std::string &) { sink.push_back({ lvl, code, std::nullopt }); }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &, const meios::operation_failure &cause, const std::string &) { sink.push_back({ lvl, code, cause }); }
};

inline meios::scratch_dir fresh_dir()
{
    std::random_device device;
    for(;;)
    {
        const std::filesystem::path candidate = std::filesystem::temp_directory_path() / ("meios-asset-" + std::to_string(device()));
        if(std::filesystem::create_directory(candidate))
            return meios::scratch_dir{ candidate };
    }
}

struct empty_asset
{
    explicit empty_asset(const std::string &extension) : tree(fresh_dir()), file(tree.path() / ("empty." + extension))
    {
        std::ofstream(file, std::ios::binary);
    }

    meios::scratch_dir tree;
    std::filesystem::path file;
};

template<typename Scanner>
void drive_refusal(Scanner &scanner, const std::string &extension)
{
    const empty_asset held{ extension };
    std::vector<captured> refused;
    meios::log_sink_f refused_log{ recorder{ refused } };

    REQUIRE(scanner.scan(meios::resolved_asset{ held.tree.path() }, refused_log).empty());
    REQUIRE(refused.size() == 1);
    REQUIRE(refused.front().lvl == meios::level::error);
    REQUIRE(refused.front().code == meios::diagnostic_code::cannot_open);
    REQUIRE(refused.front().cause.has_value());
}

// The empty regular file is the control: it keeps the refusal claim above meaningful by showing
// the parser is still reached for an asset that was genuinely read.
template<typename Scanner>
void drive_empty(Scanner &scanner, const std::string &extension, bool parser_speaks_on_empty)
{
    const empty_asset held{ extension };
    std::vector<captured> read;
    meios::log_sink_f read_log{ recorder{ read } };

    REQUIRE(scanner.scan(meios::resolved_asset{ held.file }, read_log).empty());
    REQUIRE(std::ranges::none_of(read, [](const captured &record) { return record.code == meios::diagnostic_code::cannot_open; }));
    REQUIRE(read.size() == (parser_speaks_on_empty ? 1u : 0u));
}

}

#endif
