#include "scratch_script.h"

#include <meios/io/scratch_dir.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <iterator>
#include <filesystem>
#include <system_error>

namespace
{

std::filesystem::path fresh_dir(const char *name)
{
    const std::filesystem::path path = std::filesystem::current_path() / name;
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    std::filesystem::create_directories(path, ec);
    return path;
}

std::string read_file(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// The temporary's absence is asserted by counting the directory rather than by naming the
// suffix, so the case owns no detail of how the publication names its staging file.
std::size_t entry_count(const std::filesystem::path &dir)
{
    return static_cast<std::size_t>(std::distance(std::filesystem::directory_iterator(dir), std::filesystem::directory_iterator{}));
}

std::string observed(const meios::operation_failure &cause)
{
    return std::string(meios::to_string(cause.operation)) + ' ' + std::to_string(cause.native.value()) + " (" + cause.native.category().name() + ": " + cause.native.message() + ')';
}

// Each name doubles as its own bytes, so a file holding the other entry's content is a failure
// rather than a coincidence.
void expect_suffix_siblings(const std::filesystem::path &root, const char *first, const char *second)
{
    const std::filesystem::path dir = root / "pkg" / "meshes";
    REQUIRE(meios::detail::publish_scratch_entry(root, dir / first, first).has_value());
    REQUIRE(meios::detail::publish_scratch_entry(root, dir / second, second).has_value());

    REQUIRE(read_file(dir / first) == first);
    REQUIRE(read_file(dir / second) == second);
    REQUIRE(entry_count(dir) == 2);
}

}

TEST_CASE("a published entry is replaced in place and reopened with the new bytes", "[io][scratch][publish]")
{
    const std::filesystem::path root   = fresh_dir("scratch-publish-reopen");
    const std::filesystem::path target = root / "pkg" / "meshes" / "arm.dae";

    REQUIRE(meios::detail::publish_scratch_entry(root, target, "first-bytes").has_value());
    REQUIRE(read_file(target) == "first-bytes");
    REQUIRE(meios::detail::publish_scratch_entry(root, target, "second-bytes").has_value());

    REQUIRE(read_file(target) == "second-bytes");
    REQUIRE(std::filesystem::is_regular_file(target));
    REQUIRE(entry_count(target.parent_path()) == 1);
    REQUIRE(meios::detail::scratch_staging_dir == ".meios-staging");
    REQUIRE(meios::detail::scratch_staging_file == "incoming");
    REQUIRE(std::filesystem::is_directory(root / meios::detail::scratch_staging_dir));
}

TEST_CASE("two entries whose relatives differ only by a trailing suffix each keep their own file", "[io][scratch][publish]")
{
    expect_suffix_siblings(fresh_dir("scratch-publish-suffix"), "arm.dae", "arm.dae.meios-incoming");
}

TEST_CASE("the trailing-suffix pair keeps its files in the reverse publication order", "[io][scratch][publish]")
{
    expect_suffix_siblings(fresh_dir("scratch-publish-suffix-reversed"), "arm.dae.meios-incoming", "arm.dae");
}

TEST_CASE("a refused publication keeps the prior bytes and leaves no temporary", "[io][scratch][publish]")
{
    const std::filesystem::path root   = fresh_dir("scratch-publish-rollback");
    const std::filesystem::path target = root / "pkg" / "arm.dae";
    scratch_test::scratch_script script;
    const scratch_test::scripted_scratch_operations operations{script};
    REQUIRE(meios::detail::publish_scratch_entry(root, target, "first-bytes", operations).has_value());

    script.next_publish_error                       = std::error_code{83, scratch_test::category};
    const meios::detail::scratch_step_result result = meios::detail::publish_scratch_entry(root, target, "second-bytes", operations);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().operation == meios::operation_kind::publish);
    REQUIRE(&result.error().native.category() == &scratch_test::category);
    REQUIRE(result.error().native.value() == 83);
    REQUIRE(read_file(target) == "first-bytes");
    REQUIRE(entry_count(target.parent_path()) == 1);
}

TEST_CASE("a write that cannot reach its temporary is reported as a write", "[io][scratch][publish]")
{
    const std::filesystem::path root   = fresh_dir("scratch-publish-write");
    const std::filesystem::path target = root / "pkg" / "arm.dae";
    scratch_test::scratch_script script;
    const scratch_test::scripted_scratch_operations operations{script};
    REQUIRE(meios::detail::publish_scratch_entry(root, target, "first-bytes", operations).has_value());

    script.next_write_error                         = std::error_code{89, scratch_test::category};
    const meios::detail::scratch_step_result result = meios::detail::publish_scratch_entry(root, target, "second-bytes", operations);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().operation == meios::operation_kind::write);
    REQUIRE(result.error().native.value() == 89);
    REQUIRE(read_file(target) == "first-bytes");
    REQUIRE(entry_count(target.parent_path()) == 1);
}

// The occupied path is the staging file, not the target: the publication opens a stream only on
// the staging file and reaches the target through a rename, so occupying the target would fail the
// rename instead and report publish.
TEST_CASE("a stream that cannot open its staging path is reported as an open", "[io][scratch][publish]")
{
    const std::filesystem::path root   = fresh_dir("scratch-publish-open");
    const std::filesystem::path target = root / "pkg" / "arm.dae";
    std::error_code ec;
    std::filesystem::create_directories(root / meios::detail::scratch_staging_dir / meios::detail::scratch_staging_file, ec);
    REQUIRE_FALSE(ec);

    const meios::detail::scratch_step_result result = meios::detail::publish_scratch_entry(root, target, "first-bytes");

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().operation == meios::operation_kind::open);
    REQUIRE(result.error().native.value() != 0);
    REQUIRE(static_cast<bool>(result.error().native.default_error_condition()));
}

// Whether a rename over a destination somebody still holds open succeeds is a platform fact this
// project has measured on POSIX only, so the case asserts whichever branch the host takes and
// records the native value it saw. No retry bound is derived from it here.
TEST_CASE("a replacement with the destination held open is measured, not assumed", "[io][scratch][publish]")
{
    const std::filesystem::path root   = fresh_dir("scratch-publish-contention");
    const std::filesystem::path target = root / "pkg" / "arm.dae";
    REQUIRE(meios::detail::publish_scratch_entry(root, target, "first-bytes").has_value());

    std::ifstream reader(target, std::ios::binary);
    REQUIRE(reader.is_open());
    const meios::expected<void, meios::operation_failure> result = meios::detail::publish_scratch_entry(root, target, "second-bytes");

    if(result)
    {
        WARN("a replacement over a held-open destination succeeded");
        std::ostringstream held;
        held << reader.rdbuf();
        REQUIRE(held.str() == "first-bytes");
        REQUIRE(read_file(target) == "second-bytes");
        return;
    }

    WARN("a replacement over a held-open destination was refused: " << observed(result.error()));
    REQUIRE(result.error().operation == meios::operation_kind::publish);
    REQUIRE(read_file(target) == "first-bytes");
#if defined(_WIN32)
    REQUIRE(&result.error().native.category() == &std::system_category());
#endif
    reader.close();
    REQUIRE(meios::detail::publish_scratch_entry(root, target, "second-bytes").has_value());
    REQUIRE(read_file(target) == "second-bytes");
}
