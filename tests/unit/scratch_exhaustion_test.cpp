#include "allocation_window.h"

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
using scratch_test::allocation_window;

std::filesystem::path fresh_dir(const char *name)
{
    const std::filesystem::path path = std::filesystem::current_path() / name;
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    std::filesystem::create_directories(path, ec);
    REQUIRE(std::filesystem::is_directory(path));
    return path;
}

std::filesystem::path populated_tree(const char *name)
{
    const std::filesystem::path root = fresh_dir(name);
    std::ofstream(root / "child", std::ios::binary) << "bytes";
    REQUIRE(std::filesystem::exists(root / "child"));
    return root;
}

std::size_t writing_allocations(const meios::detail::scratch_operations &operations, const std::filesystem::path &path)
{
    const allocation_window window{ false };
    operations.write_bytes(path, "bytes");
    return window_uses;
}

std::size_t removing_allocations(const meios::detail::scratch_operations &operations, const std::filesystem::path &path)
{
    const allocation_window window{ false };
    operations.remove_tree(path);
    return window_uses;
}

std::size_t stemming_allocations(const meios::detail::scratch_operations &operations)
{
    const allocation_window window{ false };
    operations.stem();
    return window_uses;
}

// The result is unwrapped here rather than assigned to a declared variable because the fallback
// expected<T, E> has no default constructor, and this stem carries a value where the two step
// verbs carry void.
std::optional<meios::operation_failure> refused_stem(const meios::detail::scratch_operations &operations)
{
    const allocation_window window{ true };
    const meios::detail::scratch_stem_result drawn = operations.stem();
    if(drawn)
        return std::nullopt;
    return drawn.error();
}

}

TEST_CASE("a write whose stream cannot obtain memory is refused rather than terminating", "[io][scratch][exhaustion]")
{
    const meios::detail::scratch_operations &operations = meios::detail::default_scratch_operations();
    const std::filesystem::path root                    = fresh_dir("scratch-exhaustion-write");
    if(writing_allocations(operations, root / "measured") == 0)
        SKIP("this standard library's output stream obtains no memory through operator new");

    const std::filesystem::path refused = root / "refused";
    meios::detail::scratch_step_result result;
    {
        const allocation_window window{ true };
        result = operations.write_bytes(refused, "bytes");
    }

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().operation == meios::operation_kind::open);
    REQUIRE(result.error().native == std::errc::not_enough_memory);
}

TEST_CASE("a removal that cannot obtain memory returns rather than terminating", "[io][scratch][exhaustion]")
{
    const meios::detail::scratch_operations &operations = meios::detail::default_scratch_operations();
    if(removing_allocations(operations, populated_tree("scratch-exhaustion-remove-measured")) == 0)
        SKIP("this standard library's recursive removal obtains no memory through operator new");

    const std::filesystem::path root = populated_tree("scratch-exhaustion-remove");
    {
        const allocation_window window{ true };
        operations.remove_tree(root);
    }

    // The surviving tree is what says the throw happened inside the removal: the recursive walk
    // fails before it unlinks anything, so an intact tree and a returning verb together mean the
    // arm answered rather than that no allocation was ever attempted.
    REQUIRE(std::filesystem::exists(root));
}

// What is driven here is the conversion, not the entropy failure that motivates it: the stem grows
// a 38-character name past every small-string buffer on the supported matrix, so the allocation is
// the reachable throw. random_device's own specified throw stays unproduced, and no host on this
// matrix can produce it.
TEST_CASE("a stem that cannot obtain memory is refused rather than terminating", "[io][scratch][exhaustion]")
{
    const meios::detail::scratch_operations &operations = meios::detail::default_scratch_operations();
    if(stemming_allocations(operations) == 0)
        SKIP("this standard library draws a scratch stem without obtaining memory");

    const std::optional<meios::operation_failure> refused = refused_stem(operations);

    REQUIRE(refused.has_value());
    REQUIRE(refused->operation == meios::operation_kind::create);
    REQUIRE(refused->native == std::errc::io_error);
}
