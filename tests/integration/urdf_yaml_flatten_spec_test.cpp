#include "urdf_yaml_flatten_fixture.h"

#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/xacro.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <utility>
#include <optional>
#include <filesystem>

namespace
{

attempt flatten(const std::filesystem::path &path, meios::eval_policy policy, const std::vector<std::filesystem::path> &roots)
{
    journal book;
    meios::log_sink_f sink{std::ref(book)};
    meios::load_options opts                                      = python_options();
    opts.eval                                                     = policy;
    opts.package_roots                                            = roots;
    meios::expected<meios::load_result, meios::load_error> loaded = meios::load(path, opts, sink);
    if(!loaded)
        return {book.errors, book.report + loaded.error().message, std::nullopt};
    return {book.errors, std::move(book.report), std::move(loaded->robot)};
}

// The diagnostic deliberately echoes the offending expression, so the leak probe looks for
// the target file's own content instead: nothing of /etc/passwd may reach any output.
void expect_refused(const attempt &got)
{
    REQUIRE_FALSE(got.robot.has_value());
    REQUIRE(got.errors >= 1);
    REQUIRE(got.report.find("uncontained-yaml-path") != std::string::npos);
    REQUIRE(got.report.find("root:") == std::string::npos);
    REQUIRE(got.report.find("/bin/") == std::string::npos);
}

}

TEST_CASE("a package-qualified yaml spec resolves through the source stack", "[urdf][yaml][flatten]")
{
    expect_shoulder(flatten(fixture("yaml_pkg/package.urdf.xacro"), meios::eval_policy::fail, fixture_roots()), 0.25, 1.5);
}

TEST_CASE("a find token written inside an expression resolves the same way", "[urdf][yaml][flatten]")
{
    expect_shoulder(flatten(fixture("yaml_pkg/find.urdf.xacro"), meios::eval_policy::fail, fixture_roots()), 0.25, 1.5);
}

// The decoy beside the top-level document carries different values, so reading the wrong
// directory would succeed with the wrong numbers rather than merely fail to resolve.
TEST_CASE("a bare yaml spec resolves against the including document, not the top-level one", "[urdf][yaml][flatten]")
{
    const attempt got = flatten(fixture("yaml_pkg/includer.urdf.xacro"), meios::eval_policy::fail, fixture_roots());
    expect_shoulder(got, 0.75, 2.5);
    REQUIRE(find_joint(*got.robot, "shoulder_joint")->origin.translation.z != Catch::Approx(0.11));
}

TEST_CASE("an absolute yaml spec fails the load and leaks no file content", "[urdf][yaml][flatten]")
{
    expect_refused(flatten(fixture("yaml_pkg/absolute.urdf.xacro"), meios::eval_policy::fail, fixture_roots()));
}

TEST_CASE("a traversal-escaping yaml spec fails the load and leaks no file content", "[urdf][yaml][flatten]")
{
    expect_refused(flatten(fixture("yaml_pkg/traversal.urdf.xacro"), meios::eval_policy::fail, fixture_roots()));
}

TEST_CASE("a yaml refusal still fails the load under the most lenient policy", "[urdf][yaml][flatten]")
{
    expect_refused(flatten(fixture("yaml_pkg/absolute.urdf.xacro"), meios::eval_policy::skip, fixture_roots()));
    expect_refused(flatten(fixture("yaml_pkg/traversal.urdf.xacro"), meios::eval_policy::skip, fixture_roots()));
}

TEST_CASE("an accepted yaml spec loads identically under the most lenient policy", "[urdf][yaml][flatten]")
{
    expect_shoulder(flatten(fixture("yaml_pkg/package.urdf.xacro"), meios::eval_policy::skip, fixture_roots()), 0.25, 1.5);
}
