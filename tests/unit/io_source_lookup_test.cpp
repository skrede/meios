#include "source_lookup_fixture.h"
#include "meios/urdf/asset_uri.h"

#include <meios/io/source_stack.h>
#include <meios/io/memory_source.h>
#include <meios/io/bundle_source.h>
#include <meios/io/package_source.h>
#include <meios/io/directory_source.h>

#include <meios/urdf/parse_context.h>

#include <meios/xacro/core_evaluator.h>

#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <filesystem>
#include <system_error>

namespace
{
using acquisition_test::answer;
using acquisition_test::real_path;
using acquisition_test::errored;
using acquisition_test::legacy_source;
using acquisition_test::loop_root;
using acquisition_test::make_tree;
using acquisition_test::record;
using acquisition_test::recorder;
using acquisition_test::seed;
using acquisition_test::typed_source;

// The stack, evaluator and recording sink one parse_context refers to, held together so a
// case states only its roots, its document and the reference it resolves.
class resolution
{
public:
    resolution(std::vector<std::filesystem::path> roots, std::filesystem::path document)
            : m_records()
            , m_log{recorder{m_records}}
            , m_sources()
            , m_eval()
            , m_ctx{m_sources,
                    m_eval,
                    m_log,
                    meios::missing_asset::skip,
                    meios::topology_policy::fail,
                    meios::material_policy::warn,
                    meios::strictness::fail,
                    std::move(document),
                    meios::completeness::none,
                    std::move(roots)}
    {
    }

    meios::source_stack &sources()
    {
        return m_sources;
    }

    const std::vector<record> &records() const
    {
        return m_records;
    }

    std::optional<std::string> resolve(const std::string &uri)
    {
        return meios::detail::resolve_asset_uri(uri, m_ctx, {});
    }

private:
    std::vector<record> m_records;
    meios::log_sink_f<recorder> m_log;
    meios::source_stack m_sources;
    meios::core_evaluator m_eval;
    meios::parse_context m_ctx;
};

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

TEST_CASE("ordered traversal recovers silently and otherwise retains the first failure", "[io][lookup]")
{
    std::vector<record> records;
    meios::log_sink_f log{recorder{records}};
    meios::source_stack recovered{typed_source{answer::failed, "", 17}, typed_source{answer::found, "recovered", 0}};
    meios::source_stack failed{typed_source{answer::failed, "", 17}, typed_source{answer::failed, "", 19}};
    meios::source_stack failed_then_miss{typed_source{answer::failed, "", 23}, typed_source{answer::absent, "", 0}};
    meios::source_stack absent{typed_source{answer::absent, "", 0}, legacy_source{""}};

    REQUIRE(recovered.try_locate("pkg", "file", log)->value().path() == "recovered");
    REQUIRE(records.empty());
    REQUIRE(failed.try_locate("pkg", "file", log).error().native.value() == 17);
    REQUIRE(failed_then_miss.try_locate("pkg", "file", log).error().native.value() == 23);
    REQUIRE_FALSE(absent.try_locate("pkg", "file", log)->has_value());
}

TEST_CASE("the asset terminal emits one structured record after native exhaustion", "[io][lookup]")
{
    resolution env{{}, "robot.urdf"};
    env.sources().push_back(meios::source_handle{typed_source{answer::failed, "", 23}});
    REQUIRE_FALSE(env.resolve("package://pkg/mesh.stl").has_value());
    REQUIRE(env.records().size() == 1);
    REQUIRE(env.records().front().code == meios::diagnostic_code::unresolved_asset);
    REQUIRE(env.records().front().cause->operation == meios::operation_kind::canonicalize);
    REQUIRE(env.records().front().cause->native == std::error_code{23, acquisition_test::lookup_error_category});
}

TEST_CASE("built-in source hits carry root-relative authorization", "[io][lookup]")
{
    const meios::scratch_dir tree = make_tree();
    seed(tree.path() / "pkg" / "meshes" / "x.stl");
    std::vector<record> records;
    meios::log_sink_f log{recorder{records}};
    meios::log_sink &seam = log;
    meios::directory_source directory{tree.path(), seam};
    meios::bundle_source bundle{tree.path(), seam};
    meios::memory_source memory{seam};
    memory.add("pkg", "meshes/x.stl", "mesh-bytes");
    const std::vector<std::optional<meios::resolved_asset>> hits = {directory.locate("pkg", "meshes/x.stl"), directory.try_locate("pkg", "meshes/x.stl").value(),
                                                                    bundle.locate("pkg", "meshes/x.stl"), memory.locate("pkg", "meshes/x.stl"), memory.locate("pkg", "meshes/x.stl")};
    for(const std::optional<meios::resolved_asset> &hit : hits)
    {
        REQUIRE(hit.has_value());
        REQUIRE(hit->source_root().has_value());
        REQUIRE(*hit->source_relative() == std::filesystem::path("pkg") / "meshes" / "x.stl");
        REQUIRE(real_path(*hit->source_root() / *hit->source_relative()) == real_path(hit->path()));
    }
    REQUIRE(*hits.front()->source_root() == meios::detail::absolute_base(tree.path()));
    REQUIRE_FALSE(directory.try_locate("absent-package", "absent-file")->has_value());
    REQUIRE_FALSE(errored(records));
}

TEST_CASE("direct asset roots separate absence, escape, recovery and native failure", "[io][lookup]")
{
    const meios::scratch_dir tree    = make_tree();
    const std::filesystem::path loop = loop_root(tree.path());
    seed(tree.path() / "meshes" / "x.stl");
    const std::filesystem::path mesh                  = real_path(tree.path() / "meshes" / "x.stl");
    const meios::detail::contained_path_result direct = meios::detail::try_contained_under(loop, mesh);
    REQUIRE_FALSE(direct.has_value());

    resolution missed{{tree.path() / "no-such-root", tree.path()}, {}};
    resolution recovered{{loop, tree.path()}, {}};
    resolution escaped{{}, {}};
    resolution vanished{{tree.path()}, {}};
    resolution terminal{{loop}, {}};

    REQUIRE(missed.resolve(mesh.string()) == std::optional{mesh.string()});
    REQUIRE(recovered.resolve(mesh.string()) == std::optional{mesh.string()});
    REQUIRE((missed.records().empty() && recovered.records().empty()));
    REQUIRE_FALSE(escaped.resolve(mesh.string()).has_value());
    REQUIRE(escaped.records().size() == 1);
    REQUIRE(escaped.records().front().code == meios::diagnostic_code::uncontained_asset);
    REQUIRE_FALSE(escaped.records().front().cause.has_value());
    REQUIRE(meios::detail::try_contained_under({}, {}).has_value());
    REQUIRE_FALSE(vanished.resolve((tree.path() / "meshes" / "gone.stl").string()).has_value());
    REQUIRE(vanished.records().empty());
    REQUIRE_FALSE(terminal.resolve(mesh.string()).has_value());
    REQUIRE(terminal.records().size() == 1);
    REQUIRE(terminal.records().front().code == meios::diagnostic_code::unresolved_asset);
    REQUIRE(terminal.records().front().cause->operation == direct.error().operation);
    REQUIRE(terminal.records().front().cause->native == direct.error().native);
}
