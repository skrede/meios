#include "scratch_source.h"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <optional>
#include <filesystem>
#include <system_error>

namespace
{

using scratch_test::probe;
using scratch_test::refusals;
using scratch_test::read_file;

// The volume the scratch root lives on is the one that decides this, and it need not be the one
// the source tree sits on. Probing inside the root also keeps concurrent stems from colliding on
// one another's probe file.
bool folds_case(const std::filesystem::path &directory)
{
    const std::filesystem::path probed = directory / "meios-case-probe";
    std::ofstream(probed, std::ios::binary) << 'x';
    std::error_code ec;
    const bool folded = std::filesystem::exists(directory / "MEIOS-CASE-PROBE", ec);
    std::filesystem::remove(probed, ec);
    return folded;
}

}

// Two differently-cased packages are two keys the normalization keeps apart and one file the
// volume folds together. That is the shape the symlinked alias cases stand in for on a host where
// no volume folds case; here it is the shape itself, on the platforms where it is the default.
TEST_CASE("a differently-cased package reaching one file is refused", "[io][scratch][case-fold]")
{
    probe held{meios::update_behavior::reject};
    held.source.add("pkg", "meshes/arm.dae", "first-bytes");
    const std::optional<meios::resolved_asset> first = held.source.locate("pkg", "meshes/arm.dae");
    REQUIRE(first.has_value());
    REQUIRE(first->source_root().has_value());

    if(!folds_case(*first->source_root()))
        SKIP("this volume distinguishes case, so two spellings cannot reach one file");

    held.source.add("Pkg", "meshes/arm.dae", "aliased-bytes");

    REQUIRE_FALSE(held.source.locate("Pkg", "meshes/arm.dae").has_value());
    REQUIRE(refusals(held.events, meios::diagnostic_code::asset_write_failed, true) == 1);
    REQUIRE(read_file(first->path()) == "first-bytes");
}
