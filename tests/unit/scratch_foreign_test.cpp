#include "allocation_window.h"

#include "meios/io/scratch_dir.h"
#include "meios/io/scratch_operations.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <fstream>
#include <optional>
#include <filesystem>
#include <system_error>

namespace
{

using scratch_test::window_uses;
using scratch_test::window_failure;
using scratch_test::allocation_window;

std::filesystem::path populated_tree(const char *name)
{
    const std::filesystem::path root = std::filesystem::current_path() / name;
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);
    std::ofstream(root / "child", std::ios::binary) << "bytes";
    REQUIRE(std::filesystem::exists(root / "child"));
    return root;
}

std::size_t stemming_allocations()
{
    const allocation_window window{ window_failure::none };
    meios::detail::default_scratch_operations().stem();
    return window_uses;
}

std::size_t writing_allocations(const std::filesystem::path &path)
{
    const allocation_window window{ window_failure::none };
    meios::detail::default_scratch_operations().write_bytes(path, "bytes");
    return window_uses;
}

std::size_t removing_allocations(const std::filesystem::path &path)
{
    const allocation_window window{ window_failure::none };
    meios::detail::default_scratch_operations().remove_tree(path);
    return window_uses;
}

std::optional<meios::operation_failure> refused_stem()
{
    const allocation_window window{ window_failure::foreign };
    const meios::detail::scratch_stem_result drawn = meios::detail::default_scratch_operations().stem();
    if(drawn)
        return std::nullopt;
    return drawn.error();
}

}

TEST_CASE("a stem is refused when the allocator throws outside the exception hierarchy", "[io][scratch][foreign]")
{
    if(stemming_allocations() == 0)
        SKIP("this standard library draws a scratch stem without obtaining memory");

    const std::optional<meios::operation_failure> refused = refused_stem();

    REQUIRE(window_uses > 0);
    REQUIRE(refused.has_value());
    REQUIRE(refused->operation == meios::operation_kind::create);
    REQUIRE(refused->native == std::errc::io_error);
}

TEST_CASE("a write is refused when the allocator throws outside the exception hierarchy", "[io][scratch][foreign]")
{
    const std::filesystem::path root = populated_tree("scratch-foreign-write");
    if(writing_allocations(root / "measured") == 0)
        SKIP("this standard library's output stream obtains no memory through operator new");

    const std::filesystem::path refused = root / "refused";
    meios::detail::scratch_step_result result;
    {
        const allocation_window window{ window_failure::foreign };
        result = meios::detail::default_scratch_operations().write_bytes(refused, "bytes");
    }

    REQUIRE(window_uses > 0);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().operation == meios::operation_kind::open);
    REQUIRE(result.error().native == std::errc::not_enough_memory);
}

TEST_CASE("a removal returns when the allocator throws outside the exception hierarchy", "[io][scratch][foreign]")
{
    if(removing_allocations(populated_tree("scratch-foreign-remove-measured")) == 0)
        SKIP("this standard library's recursive removal obtains no memory through operator new");

    const std::filesystem::path root = populated_tree("scratch-foreign-remove");
    {
        const allocation_window window{ window_failure::foreign };
        meios::detail::default_scratch_operations().remove_tree(root);
    }

    REQUIRE(window_uses > 0);
    REQUIRE(std::filesystem::exists(root / "child"));
}

// The owner is held in an optional so its destructor can run inside the window: declaring the
// window first would arm the construction instead, and declaring it second would close it before
// the owner retires.
TEST_CASE("a retiring scratch directory returns when the allocator throws outside the exception hierarchy", "[io][scratch][foreign]")
{
    if(removing_allocations(populated_tree("scratch-foreign-owner-measured")) == 0)
        SKIP("this standard library's recursive removal obtains no memory through operator new");

    const std::filesystem::path root = populated_tree("scratch-foreign-owner");
    std::optional<meios::scratch_dir> owner;
    owner.emplace(root);

    {
        const allocation_window window{ window_failure::foreign };
        owner.reset();
    }

    REQUIRE(window_uses > 0);
    REQUIRE(std::filesystem::exists(root / "child"));
}
