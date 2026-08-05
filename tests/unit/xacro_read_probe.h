#ifndef HPP_GUARD_MEIOS_UNIT_XACRO_READ_PROBE_H
#define HPP_GUARD_MEIOS_UNIT_XACRO_READ_PROBE_H

// An expansion stem needs a sink that keeps the code and the cause it is given rather than
// counting them, and a read failure it can create without privileges: a directory named as an
// include target behaves the same way on every platform.

#include <meios/io/scratch_dir.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/operation_failure.h>

#include <random>
#include <string>
#include <vector>
#include <fstream>
#include <optional>
#include <algorithm>
#include <filesystem>

namespace xacro_probe
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

inline std::size_t coded(const std::vector<captured> &records, meios::diagnostic_code code)
{
    return static_cast<std::size_t>(
        std::count_if(records.begin(), records.end(),
                      [code](const captured &r) { return r.code == code; }));
}

inline std::size_t errors(const std::vector<captured> &records)
{
    return static_cast<std::size_t>(
        std::count_if(records.begin(), records.end(),
                      [](const captured &r) { return r.lvl == meios::level::error; }));
}

inline meios::scratch_dir fresh_dir()
{
    std::random_device device;
    for(;;)
    {
        const std::filesystem::path candidate = std::filesystem::temp_directory_path() / ("meios-xacro-" + std::to_string(device()));
        if(std::filesystem::create_directory(candidate))
            return meios::scratch_dir{ candidate };
    }
}

struct directory_target
{
    explicit directory_target(const std::string &name) : tree(fresh_dir()), target(tree.path() / name)
    {
        std::filesystem::create_directory(target);
    }

    meios::scratch_dir tree;
    std::filesystem::path target;
};

// The empty regular file is the control, not a failure driver: a genuinely empty include is read
// and must still reach the parser.
struct empty_target
{
    explicit empty_target(const std::string &name) : tree(fresh_dir()), target(tree.path() / name)
    {
        std::ofstream(target, std::ios::binary);
    }

    meios::scratch_dir tree;
    std::filesystem::path target;
};

}

#endif
