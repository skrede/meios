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

}

TEST_CASE("a published entry is replaced in place and reopened with the new bytes", "[io][scratch][publish]")
{
    const std::filesystem::path target = fresh_dir("scratch-publish-reopen") / "pkg" / "meshes" / "arm.dae";

    REQUIRE(meios::detail::publish_scratch_entry(target, "first-bytes").has_value());
    REQUIRE(read_file(target) == "first-bytes");
    REQUIRE(meios::detail::publish_scratch_entry(target, "second-bytes").has_value());

    REQUIRE(read_file(target) == "second-bytes");
    REQUIRE(std::filesystem::is_regular_file(target));
    REQUIRE(entry_count(target.parent_path()) == 1);
}

TEST_CASE("a refused publication keeps the prior bytes and leaves no temporary", "[io][scratch][publish]")
{
    const std::filesystem::path target = fresh_dir("scratch-publish-rollback") / "pkg" / "arm.dae";
    scratch_test::scratch_script script;
    const scratch_test::scripted_scratch_operations operations{script};
    REQUIRE(meios::detail::publish_scratch_entry(target, "first-bytes", operations).has_value());

    script.next_publish_error                       = std::error_code{83, scratch_test::category};
    const meios::detail::scratch_step_result result = meios::detail::publish_scratch_entry(target, "second-bytes", operations);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().operation == meios::operation_kind::publish);
    REQUIRE(&result.error().native.category() == &scratch_test::category);
    REQUIRE(result.error().native.value() == 83);
    REQUIRE(read_file(target) == "first-bytes");
    REQUIRE(entry_count(target.parent_path()) == 1);
}

TEST_CASE("a write that cannot reach its temporary is reported as a write", "[io][scratch][publish]")
{
    const std::filesystem::path target = fresh_dir("scratch-publish-write") / "pkg" / "arm.dae";
    scratch_test::scratch_script script;
    const scratch_test::scripted_scratch_operations operations{script};
    REQUIRE(meios::detail::publish_scratch_entry(target, "first-bytes", operations).has_value());

    script.next_write_error                         = std::error_code{89, scratch_test::category};
    const meios::detail::scratch_step_result result = meios::detail::publish_scratch_entry(target, "second-bytes", operations);

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().operation == meios::operation_kind::write);
    REQUIRE(result.error().native.value() == 89);
    REQUIRE(read_file(target) == "first-bytes");
    REQUIRE(entry_count(target.parent_path()) == 1);
}

// Whether a rename over a destination somebody still holds open succeeds is a platform fact this
// project has measured on POSIX only, so the case asserts whichever branch the host takes and
// records the native value it saw. No retry bound is derived from it here.
TEST_CASE("a replacement with the destination held open is measured, not assumed", "[io][scratch][publish]")
{
    const std::filesystem::path target = fresh_dir("scratch-publish-contention") / "pkg" / "arm.dae";
    REQUIRE(meios::detail::publish_scratch_entry(target, "first-bytes").has_value());

    std::ifstream reader(target, std::ios::binary);
    REQUIRE(reader.is_open());
    const meios::expected<void, meios::operation_failure> result = meios::detail::publish_scratch_entry(target, "second-bytes");

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
    REQUIRE(meios::detail::publish_scratch_entry(target, "second-bytes").has_value());
    REQUIRE(read_file(target) == "second-bytes");
}
