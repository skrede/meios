#include "zip_writer_fixture.h"

#include <meios/archive/zip_writer.h>

#include <meios/bundle/result.h>
#include <meios/bundle/manifest.h>

#include <meios/diagnostic/level.h>
#include <meios/diagnostic/log_sink.h>
#include <meios/diagnostic/diagnostic_code.h>
#include <meios/diagnostic/source_location.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <filesystem>
#include <string_view>

namespace
{

using zip_fixture::write_file;
using zip_fixture::scratch_root;

struct recorder
{
    std::vector<std::string> &heard;

    void operator()(meios::level, const std::string &message)
    {
        heard.push_back(message);
    }

    void operator()(meios::level, const meios::source_location &, const std::string &message)
    {
        heard.push_back(message);
    }

    void operator()(meios::level, meios::diagnostic_code, const meios::source_location &, const std::string &message)
    {
        heard.push_back(message);
    }
};

bool heard_naming(const std::vector<std::string> &heard, std::string_view phrase, const std::string &name)
{
    for(const std::string &message : heard)
        if(message.find(phrase) != std::string::npos && message.find('\'' + name + '\'') != std::string::npos)
            return true;
    return false;
}

void check_refused(const std::filesystem::path &archive, const std::string &urdf_name, const meios::asset_manifest &manifest,
                   bool dry_run, std::string_view phrase, const std::string &named)
{
    std::vector<std::string> heard;
    meios::log_sink_f sink{ recorder{ heard } };
    meios::zip_writer writer(archive, sink);
    const meios::emit_result result = writer.write(urdf_name, "<robot/>", manifest, dry_run);
    CHECK(result.status == meios::emit_status::io_error);
    CHECK(heard_naming(heard, phrase, named));
    CHECK_FALSE(std::filesystem::exists(archive));
}

// The dry run goes first, so its no-archive check never sees what a real write left behind.
void require_refused(const std::string &suffix, const std::string &urdf_name, const std::string &dest, std::string_view phrase)
{
    const std::filesystem::path root = scratch_root(suffix);
    meios::asset_manifest manifest;
    if(!dest.empty())
        manifest.entries.push_back(meios::bundle_entry{ "package://rob/x", write_file(root / "src.stl", "X"), dest });
    const std::string &named = dest.empty() ? urdf_name : dest;
    INFO("entry name: " << named);
    check_refused(root / "rob.zip", urdf_name, manifest, true, phrase, named);
    check_refused(root / "rob.zip", urdf_name, manifest, false, phrase, named);
    std::filesystem::remove_all(root);
}

}

TEST_CASE("an entry name escaping the archive root is rejected by the containment guard, in a write and in a dry run", "[archive][zip]")
{
    require_refused("escape_up", "rob.urdf", "../escape.stl", "that escapes the bundle root");
    require_refused("escape_absolute", "rob.urdf", "/etc/passwd", "that escapes the bundle root");
}

TEST_CASE("an entry name holding a backslash is rejected on every platform, in a write and in a dry run", "[archive][zip]")
{
    require_refused("backslash_up", "rob.urdf", "a\\..\\..\\x", "holds a backslash");
    require_refused("backslash_inner", "rob.urdf", "a\\b", "holds a backslash");
    require_refused("backslash_lead", "rob.urdf", "\\x", "holds a backslash");
}

TEST_CASE("an entry name beginning with a drive letter and colon is rejected on every platform, in a write and in a dry run", "[archive][zip]")
{
    require_refused("drive_urdf", "D:x.urdf", "", "begins with a drive letter and colon");
    require_refused("drive_slash", "rob.urdf", "d:/x", "begins with a drive letter and colon");
    require_refused("drive_dot", "rob.urdf", "./D:x", "begins with a drive letter and colon");
}

TEST_CASE("an entry name with a root as the platform reads it is rejected by the containment guard, in a write and in a dry run", "[archive][zip]")
{
    require_refused("rooted", "rob.urdf", "/x", "that escapes the bundle root");
}

TEST_CASE("an entry name that names the archive root itself is rejected on every platform, in a write and in a dry run", "[archive][zip]")
{
    require_refused("itself_urdf", ".", "", "names the bundle root itself");
    require_refused("itself_up", "rob.urdf", "x/..", "names the bundle root itself");
    require_refused("itself_slash", "rob.urdf", "./", "names the bundle root itself");
    require_refused("itself_dot_up", "rob.urdf", "a/./..", "names the bundle root itself");
    require_refused("itself_empty", "", "", "names the bundle root itself");
}

TEST_CASE("an entry name that names a directory is rejected on every platform, in a write and in a dry run", "[archive][zip]")
{
    require_refused("directory_urdf", "rob/", "", "names a directory, not a file");
    require_refused("directory_slash", "rob.urdf", "x/", "names a directory, not a file");
    require_refused("directory_dot", "rob.urdf", "x/.", "names a directory, not a file");
    require_refused("directory_up", "rob.urdf", "a/b/..", "names a directory, not a file");
    require_refused("directory_package", "rob.urdf", "meshes/p/", "names a directory, not a file");
}
