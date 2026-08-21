#include <meios/urdf.h>
#include <meios/model.h>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <filesystem>

namespace
{

using load_outcome = meios::expected<meios::load_result, meios::load_error>;

// The entry point a consumer reaches, driven with the caller argument a consumer would hand it,
// rather than a scope seeded in the test: the caller seed is only reachable this way.
load_outcome loaded(const std::map<std::string, std::string> &args)
{
    meios::load_options opts;
    opts.args = args;
    return meios::load(std::filesystem::path{ MEIOS_URDF_FIXTURE_DIR } / "arg_domain.xacro", opts);
}

}

TEST_CASE("a caller's boolean spelling reaches the model as the caller wrote it", "[urdf][arg]")
{
    const load_outcome out = loaded({ { "flag", "false" } });

    REQUIRE(out.has_value());
    REQUIRE(out->robot.links.size() == 1);
    REQUIRE(out->robot.links[0].name == "caller_false");
}

TEST_CASE("the declared default's spelling reaches the model when the caller supplies none",
          "[urdf][arg]")
{
    const load_outcome out = loaded({});

    REQUIRE(out.has_value());
    REQUIRE(out->robot.links.size() == 1);
    REQUIRE(out->robot.links[0].name == "caller_true");
}
