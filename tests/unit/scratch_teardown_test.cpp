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

std::filesystem::path populated_tree(const char *name)
{
    const std::filesystem::path root = std::filesystem::current_path() / name;
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);
    std::ofstream(root / "child", std::ios::binary) << "bytes";
    return root;
}

std::size_t removing_allocations(const std::filesystem::path &path)
{
    const allocation_window window{ false };
    meios::detail::default_scratch_operations().remove_tree(path);
    return window_uses;
}

}

// The owner is held in an optional so its destructor can run inside the window: declaring the
// window first would arm the construction instead, and declaring it second would close it before
// the owner retires.
TEST_CASE("a scratch directory whose removal cannot obtain memory does not terminate", "[io][scratch][teardown]")
{
    if(removing_allocations(populated_tree("scratch-teardown-measured")) == 0)
        SKIP("this standard library's recursive removal obtains no memory through operator new");

    const std::filesystem::path root = populated_tree("scratch-teardown");
    std::optional<meios::scratch_dir> owner;
    owner.emplace(root);

    {
        const allocation_window window{ true };
        owner.reset();
    }

    // A destructor is noexcept, so reaching this line at all is the assertion: an escaping throw
    // would have terminated the process. The surviving tree says the throw really happened.
    REQUIRE(std::filesystem::exists(root));
}
