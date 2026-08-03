#ifndef HPP_GUARD_MEIOS_TEST_SCRATCH_SOURCE_H
#define HPP_GUARD_MEIOS_TEST_SCRATCH_SOURCE_H

#include "scratch_capture.h"

#include <meios/io.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>
#include <fstream>
#include <optional>
#include <iterator>
#include <filesystem>

namespace scratch_test
{

// The sink is consumed by the source, so declaration order here is construction order and the
// length triangle yields to it.
struct probe
{
    explicit probe(meios::update_behavior behavior)
            : events()
            , sink(capture{events})
            , source(sink, behavior)
    {
    }

    event_log events;
    meios::log_sink_f<capture> sink;
    meios::memory_source source;
};

inline std::string read_file(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

inline std::filesystem::path locate_path(meios::memory_source &source, const char *relative)
{
    const std::optional<meios::resolved_asset> hit = source.locate("pkg", relative);
    REQUIRE(hit.has_value());
    return hit->path();
}

// The code is a parameter because the source refuses at two boundaries under two codes, and a
// caller counting one of them must not be handed the other's total.
inline std::size_t refusals(const event_log &events, meios::diagnostic_code code, bool with_cause)
{
    std::size_t total = 0;
    for(const event &recorded : events)
        if(recorded.lvl == meios::level::error && recorded.code == code && recorded.cause.has_value() == with_cause)
            ++total;
    return total;
}

inline std::size_t entry_count(const std::filesystem::path &dir)
{
    return static_cast<std::size_t>(std::distance(std::filesystem::directory_iterator(dir), std::filesystem::directory_iterator{}));
}

}

#endif
