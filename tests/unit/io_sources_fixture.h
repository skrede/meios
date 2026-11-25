#ifndef HPP_GUARD_MEIOS_UNIT_IO_SOURCES_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_IO_SOURCES_FIXTURE_H

#include <meios/io/memory_source.h>
#include <meios/io/resolved_asset.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/source_location.h>

#include <catch2/catch_test_macros.hpp>

#include <random>
#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <optional>
#include <filesystem>

namespace
{

struct event
{
    meios::level lvl;
    meios::diagnostic_code code;
};

using event_log = std::vector<event>;

struct capture
{
    event_log &events;

    void operator()(meios::level lvl, const std::string &)
    {
        events.push_back({lvl, meios::diagnostic_code::unspecified});
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &)
    {
        events.push_back({lvl, meios::diagnostic_code::unspecified});
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &, const std::string &)
    {
        events.push_back({lvl, code});
    }
};

inline std::size_t count_code(const event_log &events, meios::level lvl, meios::diagnostic_code code)
{
    std::size_t total = 0;
    for(const event &recorded : events)
        if(recorded.lvl == lvl && recorded.code == code)
            ++total;
    return total;
}

inline std::filesystem::path locate_path(meios::memory_source &source, const char *relative)
{
    const std::optional<meios::resolved_asset> hit = source.locate("pkg", relative);
    REQUIRE(hit.has_value());
    return hit->path();
}

inline std::filesystem::path fresh_dir()
{
    std::random_device device;
    for(;;)
    {
        std::filesystem::path candidate = std::filesystem::temp_directory_path() / ("meios-io-" + std::to_string(device()));
        if(!std::filesystem::exists(candidate))
            return candidate;
    }
}

inline std::filesystem::path seed_root()
{
    std::filesystem::path root = fresh_dir();
    std::filesystem::create_directories(root / "pkg" / "meshes");
    std::ofstream(root / "pkg" / "meshes" / "x.stl") << "mesh-bytes";
    return root;
}

}

#endif
