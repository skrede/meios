#include "scratch_source.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <optional>
#include <filesystem>

// The temporary-directory environment is process-wide, and this target points it at a symbolic
// link rather than at a plain directory — the shape a system temporary directory reached through
// one has. That is why this is a target of its own rather than a case beside ones that need an
// ordinary temporary directory.

namespace
{

using scratch_test::probe;

std::filesystem::path configured_temporary_directory()
{
    const char *named = std::getenv("TMPDIR");
    REQUIRE(named != nullptr);
    const std::filesystem::path configured{named};
    REQUIRE(std::filesystem::is_symlink(configured));
    return configured;
}

std::optional<meios::resolved_asset> one_served_entry(probe &held)
{
    held.source.add("pkg", "meshes/arm.dae", "first-bytes");
    return held.source.locate("pkg", "meshes/arm.dae");
}

}

TEST_CASE("a resolution lies under the scratch root it reports when the temporary directory is a symbolic link",
          "[io][scratch][symlinked]")
{
    const std::filesystem::path configured = configured_temporary_directory();

    probe held{meios::update_behavior::reject};
    const std::optional<meios::resolved_asset> hit = one_served_entry(held);
    REQUIRE(hit.has_value());
    REQUIRE(hit->source_root().has_value());

    const std::filesystem::path &root    = *hit->source_root();
    const std::filesystem::path relative = hit->path().lexically_relative(root);
    INFO("configured " << configured << ", root " << root << ", path " << hit->path() << ", relative " << relative);

    CHECK(std::filesystem::is_directory(root));
    CHECK(std::filesystem::is_regular_file(hit->path()));
    CHECK_FALSE(relative.empty());
    CHECK(*relative.begin() != std::filesystem::path{".."});
}

TEST_CASE("the reported scratch root resolves the link rather than repeating it",
          "[io][scratch][symlinked]")
{
    const std::filesystem::path configured = configured_temporary_directory();

    probe held{meios::update_behavior::reject};
    const std::optional<meios::resolved_asset> hit = one_served_entry(held);
    REQUIRE(hit.has_value());
    REQUIRE(hit->source_root().has_value());

    const std::filesystem::path under = hit->source_root()->lexically_relative(configured);
    INFO("configured " << configured << ", root " << *hit->source_root() << ", relative " << under);

    CHECK((under.empty() || *under.begin() == std::filesystem::path{".."}));
}
