#include "source_lookup_fixture.h"
#include "meios/urdf/asset_uri.h"

#include <meios/io/scratch_dir.h>
#include <meios/io/source_stack.h>
#include <meios/io/package_source.h>
#include <meios/io/directory_source.h>

#include <meios/urdf/parse_context.h>

#include <meios/xacro/core_evaluator.h>

#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <optional>
#include <algorithm>
#include <filesystem>

namespace
{
using acquisition_test::answer;
using acquisition_test::legacy_source;
using acquisition_test::record;
using acquisition_test::recorder;
using acquisition_test::typed_source;

}

static_assert(meios::package_source<legacy_source>);
static_assert(meios::package_source<typed_source>);
static_assert(!meios::provides_typed_lookup<legacy_source>);
static_assert(meios::provides_typed_lookup<typed_source>);

TEST_CASE("typed lookup survives erasure while legacy lookup adapts", "[io][lookup]")
{
    meios::source_handle legacy{legacy_source{"legacy"}};
    meios::source_handle found{typed_source{answer::found, "typed", 0}};
    meios::source_handle absent{typed_source{answer::absent, "", 0}};
    meios::source_handle failed{typed_source{answer::failed, "", 17}};

    REQUIRE(legacy.try_locate("pkg", "file")->value().path() == "legacy");
    REQUIRE(found.try_locate("pkg", "file")->value().path() == "typed");
    REQUIRE_FALSE(absent.try_locate("pkg", "file")->has_value());
    REQUIRE(failed.try_locate("pkg", "file").error().native.value() == 17);
}

TEST_CASE("ordered lookup preserves first hit and successful shadows", "[io][lookup]")
{
    std::vector<record> records;
    meios::log_sink_f log{recorder{records}};
    meios::source_stack stack{legacy_source{"high"}, typed_source{answer::failed, "", 31}, typed_source{answer::found, "low", 0}, typed_source{answer::found, "lowest", 0}};

    const meios::source_lookup_result hit = stack.try_locate("pkg", "file", log);

    REQUIRE(hit->value().path() == "high");
    REQUIRE(records.size() == 2);
    REQUIRE(records.front().level == meios::level::info);
    REQUIRE(records.back().level == meios::level::info);
}

TEST_CASE("a later hit discards an earlier failure without a diagnostic", "[io][lookup]")
{
    std::vector<record> records;
    meios::log_sink_f log{recorder{records}};
    meios::source_stack stack{typed_source{answer::failed, "", 17}, typed_source{answer::found, "recovered", 0}};

    const meios::source_lookup_result hit = stack.try_locate("pkg", "file", log);

    REQUIRE(hit->value().path() == "recovered");
    REQUIRE(records.empty());
}

TEST_CASE("exhaustion retains the first failure and all miss stays ordinary", "[io][lookup]")
{
    meios::log_sink log;
    meios::source_stack failed{typed_source{answer::failed, "", 17}, typed_source{answer::failed, "", 19}};
    meios::source_stack failed_then_miss{typed_source{answer::failed, "", 23}, typed_source{answer::absent, "", 0}};
    meios::source_stack absent{typed_source{answer::absent, "", 0}, legacy_source{""}};

    REQUIRE(failed.try_locate("pkg", "file", log).error().native.value() == 17);
    REQUIRE(failed_then_miss.try_locate("pkg", "file", log).error().native.value() == 23);
    REQUIRE_FALSE(absent.try_locate("pkg", "file", log)->has_value());
}

TEST_CASE("the asset terminal emits one structured record after native exhaustion", "[io][lookup]")
{
    std::vector<record> records;
    meios::log_sink_f log{recorder{records}};
    meios::source_stack sources{typed_source{answer::failed, "", 23}};
    meios::core_evaluator eval;
    meios::parse_context ctx{
            sources, eval, log, meios::missing_asset::skip, meios::topology_policy::fail, meios::material_policy::warn, meios::strictness::fail, "robot.urdf", meios::completeness::none,
            {}};

    REQUIRE_FALSE(meios::detail::resolve_asset_uri("package://pkg/mesh.stl", ctx, {}).has_value());
    REQUIRE(records.size() == 1);
    REQUIRE(records.front().code == meios::diagnostic_code::unresolved_asset);
    REQUIRE(records.front().cause->operation == meios::operation_kind::canonicalize);
    REQUIRE(records.front().cause->native.value() == 23);
    REQUIRE(&records.front().cause->native.category() == &acquisition_test::lookup_error_category);
}

TEST_CASE("a document with no path contains nothing", "[io][lookup]")
{
    std::error_code error;
    const std::filesystem::path parent = std::filesystem::temp_directory_path(error);
    REQUIRE_FALSE(error);
    const auto root = meios::detail::create_scratch_root(parent, error);
    REQUIRE(root.has_value());
    meios::scratch_dir tree{*root};
    std::filesystem::create_directories(tree.path() / "meshes");
    std::ofstream(tree.path() / "meshes" / "x.stl") << "mesh-bytes";

    std::vector<record> records;
    meios::log_sink_f log{recorder{records}};
    meios::source_stack sources;
    meios::core_evaluator eval;
    meios::parse_context ctx{
            sources, eval, log, meios::missing_asset::skip, meios::topology_policy::fail, meios::material_policy::warn, meios::strictness::fail, {}, meios::completeness::none, {}};

    const std::filesystem::path saved = std::filesystem::current_path();
    std::filesystem::current_path(tree.path());
    const std::optional<std::string> resolved = meios::detail::resolve_asset_uri("meshes/x.stl", ctx, {});
    std::filesystem::current_path(saved);

    REQUIRE_FALSE(resolved.has_value());
    REQUIRE(records.size() == 1);
    REQUIRE(records.front().code == meios::diagnostic_code::uncontained_asset);
}

TEST_CASE("a relative configured root still contains an in-root asset", "[io][lookup]")
{
    std::error_code error;
    const std::filesystem::path parent = std::filesystem::temp_directory_path(error);
    REQUIRE_FALSE(error);
    const auto root = meios::detail::create_scratch_root(parent, error);
    REQUIRE(root.has_value());
    meios::scratch_dir tree{*root};
    std::filesystem::create_directories(tree.path() / "pkg" / "meshes");
    std::ofstream(tree.path() / "pkg" / "meshes" / "x.stl") << "mesh-bytes";
    const std::filesystem::path mesh = std::filesystem::weakly_canonical(tree.path() / "pkg" / "meshes" / "x.stl", error);
    REQUIRE_FALSE(error);

    std::vector<record> records;
    meios::log_sink_f log{recorder{records}};
    meios::source_stack sources;
    meios::core_evaluator eval;
    meios::parse_context ctx{sources,
                             eval,
                             log,
                             meios::missing_asset::skip,
                             meios::topology_policy::fail,
                             meios::material_policy::warn,
                             meios::strictness::fail,
                             {},
                             meios::completeness::none,
                             {tree.path().filename()}};

    const std::filesystem::path saved = std::filesystem::current_path();
    std::filesystem::current_path(tree.path().parent_path());
    const std::optional<std::string> resolved = meios::detail::resolve_asset_uri(mesh.string(), ctx, {});
    std::filesystem::current_path(saved);

    const bool errored = std::any_of(records.begin(), records.end(), [](const record &noted) { return noted.level == meios::level::error; });
    REQUIRE(resolved == std::optional<std::string>{mesh.string()});
    REQUIRE_FALSE(errored);
}

TEST_CASE("a bare relative document reaches the sibling it was written beside", "[io][lookup]")
{
    std::error_code error;
    const std::filesystem::path parent = std::filesystem::temp_directory_path(error);
    REQUIRE_FALSE(error);
    const auto root = meios::detail::create_scratch_root(parent, error);
    REQUIRE(root.has_value());
    meios::scratch_dir tree{*root};
    std::filesystem::create_directories(tree.path() / "meshes");
    std::ofstream(tree.path() / "meshes" / "x.stl") << "mesh-bytes";
    const std::filesystem::path mesh = std::filesystem::weakly_canonical(tree.path() / "meshes" / "x.stl", error);
    REQUIRE_FALSE(error);

    std::vector<record> records;
    meios::log_sink_f log{recorder{records}};
    meios::source_stack sources;
    meios::core_evaluator eval;
    meios::parse_context ctx{
            sources, eval, log, meios::missing_asset::skip, meios::topology_policy::fail, meios::material_policy::warn, meios::strictness::fail, "robot.urdf", meios::completeness::none,
            {}};

    const std::filesystem::path saved = std::filesystem::current_path();
    std::filesystem::current_path(tree.path());
    const std::optional<std::string> resolved = meios::detail::resolve_asset_uri("meshes/x.stl", ctx, {});
    std::filesystem::current_path(saved);

    REQUIRE(resolved == std::optional<std::string>{mesh.string()});
    REQUIRE(records.empty());
}

TEST_CASE("canonicalization failure remains distinct from containment rejection", "[io][lookup]")
{
    std::error_code error;
    const std::filesystem::path parent = std::filesystem::temp_directory_path(error);
    REQUIRE_FALSE(error);
    const auto root = meios::detail::create_scratch_root(parent, error);
    REQUIRE(root.has_value());
    meios::scratch_dir tree{*root};
    const std::filesystem::path loop = tree.path() / "loop";
    std::filesystem::create_directory_symlink(loop, loop, error);
    REQUIRE_FALSE(error);
    const auto failed   = meios::detail::try_contained_under(tree.path(), loop / "child");
    const auto rejected = meios::detail::try_contained_under({}, {});

    REQUIRE_FALSE(failed.has_value());
    REQUIRE(failed.error().operation == meios::operation_kind::canonicalize);
    REQUIRE(static_cast<bool>(failed.error().native));
    REQUIRE(rejected.has_value());
    REQUIRE_FALSE(rejected->has_value());

    meios::log_sink log;
    meios::directory_source directory{tree.path(), log};
    const meios::source_lookup_result missing = directory.try_locate("missing-package", "missing-file");
    REQUIRE(missing.has_value());
    REQUIRE_FALSE(missing->has_value());
}
