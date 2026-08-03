#include "scratch_script.h"

#include <meios/io/scratch_dir.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstddef>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <system_error>

namespace
{

std::filesystem::path fresh_parent(const char *name)
{
    const std::filesystem::path path = std::filesystem::current_path() / name;
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
    std::filesystem::create_directories(path, ec);
    return path;
}

// One candidate more than the bound, so a case can show the loop stopping at the bound rather
// than running past it into a name that was left free.
struct sweep
{
    explicit sweep(const char *name)
            : parent(fresh_parent(name))
            , script()
    {
        for(int i = 0; i <= meios::detail::scratch_root_attempts; ++i)
            script.stems.push_back("candidate-" + std::to_string(i));
    }

    std::filesystem::path candidate(std::size_t index) const
    {
        return parent / script.stems[index];
    }

    void occupy(int count) const
    {
        for(int i = 0; i < count; ++i)
            std::filesystem::create_directory(candidate(static_cast<std::size_t>(i)));
    }

    meios::detail::scratch_root_result run()
    {
        const scratch_test::scripted_scratch_operations operations{script};
        return meios::detail::create_scratch_root(parent, operations);
    }

    std::filesystem::path parent;
    scratch_test::scratch_script script;
};

void require_collision_refusal(const meios::detail::scratch_root_result &result)
{
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().operation == meios::operation_kind::create);
    REQUIRE(result.error().native.default_error_condition() == std::errc::file_exists);
}

void require_second_candidate(sweep &fixture)
{
    const meios::detail::scratch_root_result result = fixture.run();
    REQUIRE(result.has_value());
    REQUIRE(result->parent_path() == fixture.parent);
    REQUIRE(result->filename() == fixture.script.stems[1]);
}

}

TEST_CASE("an exhausted candidate sequence is refused as a name collision", "[io][scratch][setup]")
{
    sweep fixture{"scratch-setup-exhausted"};
    fixture.occupy(meios::detail::scratch_root_attempts);

    require_collision_refusal(fixture.run());
}

TEST_CASE("the candidate loop takes the first free name and exhausts at its bound", "[io][scratch][setup]")
{
    for(int taken : {0, 1, meios::detail::scratch_root_attempts - 1})
    {
        INFO("occupied candidates " << taken);
        sweep fixture{"scratch-setup-sweep"};
        fixture.occupy(taken);

        const meios::detail::scratch_root_result result = fixture.run();
        REQUIRE(result.has_value());
        REQUIRE(result->parent_path() == fixture.parent);
        REQUIRE(result->filename() == fixture.script.stems[static_cast<std::size_t>(taken)]);
    }

    sweep exhausted{"scratch-setup-sweep"};
    exhausted.occupy(meios::detail::scratch_root_attempts);

    require_collision_refusal(exhausted.run());
}

TEST_CASE("a candidate name held by a directory is retried at the next candidate", "[io][scratch][setup]")
{
    sweep fixture{"scratch-setup-held-directory"};
    std::filesystem::create_directory(fixture.candidate(0));

    require_second_candidate(fixture);
}

TEST_CASE("a candidate name held by a regular file is retried at the next candidate", "[io][scratch][setup]")
{
    sweep fixture{"scratch-setup-held-file"};
    std::ofstream(fixture.candidate(0)) << "taken";

    require_second_candidate(fixture);
}

#ifndef _WIN32
TEST_CASE("a candidate name held by a symlink to a directory is retried, not taken over", "[io][scratch][setup]")
{
    sweep fixture{"scratch-setup-held-symlink"};
    const std::filesystem::path target = fixture.parent / "link-target";
    std::filesystem::create_directory(target);
    std::filesystem::create_directory_symlink(target, fixture.candidate(0));

    require_second_candidate(fixture);
    REQUIRE(std::filesystem::is_empty(target));
}

TEST_CASE("a candidate name held by a dangling symlink is retried at the next candidate", "[io][scratch][setup]")
{
    sweep fixture{"scratch-setup-dangling-symlink"};
    std::filesystem::create_symlink(fixture.parent / "absent-target", fixture.candidate(0));

    require_second_candidate(fixture);
}
#endif

TEST_CASE("a root that could not be narrowed is removed and refused", "[io][scratch][setup]")
{
    sweep fixture{"scratch-setup-narrowing"};
    fixture.script.narrow_error = std::error_code{57, scratch_test::category};

    const meios::detail::scratch_root_result result = fixture.run();

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().operation == meios::operation_kind::permissions);
    REQUIRE(&result.error().native.category() == &scratch_test::category);
    REQUIRE(result.error().native.value() == 57);
    REQUIRE_FALSE(std::filesystem::exists(fixture.candidate(0)));
    REQUIRE(std::find(fixture.script.calls.begin(), fixture.script.calls.end(), "remove") != fixture.script.calls.end());
}

TEST_CASE("a stem that cannot be drawn is answered as a value", "[io][scratch][setup]")
{
    sweep fixture{"scratch-setup-entropy"};
    fixture.script.stem_error = std::error_code{61, scratch_test::category};

    REQUIRE_NOTHROW(fixture.run());
    const meios::detail::scratch_root_result result = fixture.run();

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().operation == meios::operation_kind::create);
    REQUIRE(result.error().native.value() == 61);
}

TEST_CASE("a collision ahead of a permanent creation failure reports the permanent cause", "[io][scratch][setup]")
{
    sweep fixture{"scratch-setup-permanent"};
    fixture.occupy(1);
    fixture.script.next_create_error = std::error_code{71, scratch_test::category};

    const meios::detail::scratch_root_result result = fixture.run();

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().operation == meios::operation_kind::create);
    REQUIRE(result.error().native.value() == 71);
    REQUIRE(result.error().native.default_error_condition() != std::errc::file_exists);
}

TEST_CASE("a collision ahead of a narrowing failure reports the narrowing step", "[io][scratch][setup]")
{
    sweep fixture{"scratch-setup-collision-narrowing"};
    fixture.occupy(1);
    fixture.script.narrow_error = std::error_code{73, scratch_test::category};

    const meios::detail::scratch_root_result result = fixture.run();

    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().operation == meios::operation_kind::permissions);
    REQUIRE(result.error().operation != meios::operation_kind::create);
    REQUIRE(result.error().native.value() == 73);
}
