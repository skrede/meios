#include "scratch_capture.h"

#include <meios/io.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>
#include <fstream>
#include <optional>
#include <iterator>
#include <filesystem>
#include <system_error>

namespace
{

// The sink is consumed by the source, so declaration order here is construction order and the
// length triangle yields to it.
struct probe
{
    explicit probe(meios::update_behavior behavior)
            : events()
            , sink(scratch_test::capture{events})
            , source(sink, behavior)
    {
    }

    scratch_test::event_log events;
    meios::log_sink_f<scratch_test::capture> sink;
    meios::memory_source source;
};

std::string read_file(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

std::filesystem::path locate_path(meios::memory_source &source, const char *relative)
{
    const std::optional<meios::resolved_asset> hit = source.locate("pkg", relative);
    REQUIRE(hit.has_value());
    return hit->path();
}

std::size_t refusals(const scratch_test::event_log &events, bool with_cause)
{
    std::size_t total = 0;
    for(const scratch_test::event &recorded : events)
        if(recorded.lvl == meios::level::error && recorded.code == meios::diagnostic_code::asset_write_failed && recorded.cause.has_value() == with_cause)
            ++total;
    return total;
}

std::size_t entry_count(const std::filesystem::path &dir)
{
    return static_cast<std::size_t>(std::distance(std::filesystem::directory_iterator(dir), std::filesystem::directory_iterator{}));
}

}

TEST_CASE("the entry-update vocabulary spells itself", "[io][scratch][replace]")
{
    REQUIRE(meios::to_string(meios::update_behavior::reject) == "reject");
    REQUIRE(meios::to_string(meios::update_behavior::replace) == "replace");
}

TEST_CASE("a duplicate key is refused and the first bytes keep resolving", "[io][scratch][replace]")
{
    probe held{meios::update_behavior::reject};
    held.source.add("pkg", "meshes/arm.dae", "first-bytes");
    held.source.add("pkg", "meshes/arm.dae", "second-bytes");

    REQUIRE(refusals(held.events, false) == 1);
    REQUIRE(read_file(locate_path(held.source, "meshes/arm.dae")) == "first-bytes");
}

TEST_CASE("a source built to replace accepts the second offer", "[io][scratch][replace]")
{
    probe held{meios::update_behavior::replace};
    held.source.add("pkg", "meshes/arm.dae", "first-bytes");
    held.source.add("pkg", "meshes/arm.dae", "second-bytes");

    REQUIRE(refusals(held.events, false) == 0);
    REQUIRE(read_file(locate_path(held.source, "meshes/arm.dae")) == "second-bytes");
}

TEST_CASE("a replacement publishes to the path a resolution already handed out", "[io][scratch][replace]")
{
    probe held{meios::update_behavior::replace};
    held.source.add("pkg", "meshes/arm.dae", "first-bytes");

    const std::filesystem::path before = locate_path(held.source, "meshes/arm.dae");
    held.source.add("pkg", "meshes/arm.dae", "second-bytes");
    const std::filesystem::path after = locate_path(held.source, "meshes/arm.dae");

    REQUIRE(after == before);
    REQUIRE(read_file(before) == "second-bytes");
}

TEST_CASE("a replaced entry leaves one regular file and no generation directory", "[io][scratch][replace]")
{
    probe held{meios::update_behavior::replace};
    held.source.add("pkg", "meshes/arm.dae", "first-bytes");
    const std::filesystem::path located = locate_path(held.source, "meshes/arm.dae");
    held.source.add("pkg", "meshes/arm.dae", "second-bytes");

    const std::filesystem::path dir = located.parent_path();
    REQUIRE(entry_count(dir) == 1);
    REQUIRE(std::filesystem::is_regular_file(dir / "arm.dae"));
}

TEST_CASE("a sibling still resolves by a plain relative join after a replacement", "[io][scratch][replace]")
{
    probe held{meios::update_behavior::replace};
    held.source.add("pkg", "meshes/arm.dae", "collada-bytes");
    held.source.add("pkg", "meshes/arm.mtl", "material-bytes");

    const std::filesystem::path model   = locate_path(held.source, "meshes/arm.dae");
    const std::filesystem::path sibling = locate_path(held.source, "meshes/arm.mtl");
    held.source.add("pkg", "meshes/arm.dae", "replaced-bytes");

    REQUIRE(sibling == model.parent_path() / "arm.mtl");
    REQUIRE(read_file(model) == "replaced-bytes");
    REQUIRE(read_file(sibling) == "material-bytes");
    REQUIRE(entry_count(model.parent_path()) == 2);
    for(const std::filesystem::directory_entry &child : std::filesystem::directory_iterator(model.parent_path()))
        REQUIRE(child.is_regular_file());
}

TEST_CASE("an entry deleted outside the source is rematerialized at the same path", "[io][scratch][replace]")
{
    probe held{meios::update_behavior::reject};
    held.source.add("pkg", "meshes/arm.dae", "first-bytes");

    const std::filesystem::path before = locate_path(held.source, "meshes/arm.dae");
    std::filesystem::remove(before);
    REQUIRE_FALSE(std::filesystem::exists(before));
    const std::filesystem::path after = locate_path(held.source, "meshes/arm.dae");

    REQUIRE(after == before);
    REQUIRE(read_file(after) == "first-bytes");
}

#ifndef _WIN32
namespace
{

void set_directory_mode(const std::filesystem::path &dir, std::filesystem::perms mode)
{
    std::error_code ec;
    std::filesystem::permissions(dir, mode, std::filesystem::perm_options::replace, ec);
}

// Windows models directory permissions as a read-only attribute, which denies nothing, so the
// case below is compiled out there rather than skipped. The denial is also a no-op for a
// privileged process, which this reports so the case can warn off instead of passing silently.
bool deny_writes(const std::filesystem::path &dir)
{
    set_directory_mode(dir, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec);
    const std::filesystem::path candidate = dir / "denial-probe";
    std::ofstream out(candidate);
    if(!out.is_open())
        return true;
    out.close();
    std::error_code ec;
    std::filesystem::remove(candidate, ec);
    return false;
}

}

TEST_CASE("a replacement that cannot be written keeps the last successful write", "[io][scratch][replace]")
{
    probe held{meios::update_behavior::replace};
    held.source.add("pkg", "meshes/arm.dae", "first-bytes");
    const std::filesystem::path located = locate_path(held.source, "meshes/arm.dae");

    const bool denied = deny_writes(located.parent_path());
    if(denied)
        held.source.add("pkg", "meshes/arm.dae", "second-bytes");
    set_directory_mode(located.parent_path(), std::filesystem::perms::owner_all);
    if(!denied)
    {
        WARN("this process writes into a directory it holds no write permission on; assertion skipped");
        return;
    }

    REQUIRE(refusals(held.events, true) == 1);
    REQUIRE(read_file(located) == "first-bytes");
    REQUIRE(read_file(locate_path(held.source, "meshes/arm.dae")) == "first-bytes");
}
#endif
