#include "scratch_source.h"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <filesystem>
#include <system_error>

namespace
{

using scratch_test::probe;
using scratch_test::refusals;
using scratch_test::read_file;
using scratch_test::locate_path;

void set_directory_mode(const std::filesystem::path &dir, std::filesystem::perms mode)
{
    std::error_code ec;
    std::filesystem::permissions(dir, mode, std::filesystem::perm_options::replace, ec);
}

// The denial is a no-op for a privileged process, which this reports so the case can warn off
// instead of passing silently.
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

TEST_CASE("a replacement that cannot be written keeps the last successful write", "[io][scratch][denial]")
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

    REQUIRE(refusals(held.events, meios::diagnostic_code::asset_write_failed, true) == 1);
    REQUIRE(read_file(located) == "first-bytes");
    REQUIRE(read_file(locate_path(held.source, "meshes/arm.dae")) == "first-bytes");
}
