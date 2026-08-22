#include <meios/urdf.h>
#include <meios/model.h>

#include <meios/bundle/flatten.h>

#include <meios/diagnostic/log_sink.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <filesystem>

#ifndef MEIOS_FLATTEN_GOLDEN_DIR
    #error "MEIOS_FLATTEN_GOLDEN_DIR must name the flatten baseline directory"
#endif
#ifndef MEIOS_CORPUS_KNOWN_GOOD
    #error "MEIOS_CORPUS_KNOWN_GOOD must name the pinned known-good description"
#endif

namespace
{

std::string slurp(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// Drives the exact load -> flatten sequence the CLI's flatten verb does, into a
// string, so the bytes asserted here are the bytes a consumer would get. The pinned
// description's package:// meshes are not fetched with it, and the subject here is
// the flattened bytes rather than asset resolution, so the asset policy is lowered to
// warn and the discard sink swallows the notes it produces.
std::string flatten_known_good()
{
    meios::log_sink discard;
    meios::load_options opts;
    opts.on_missing = meios::missing_asset::warn;
    const std::filesystem::path source{ MEIOS_CORPUS_KNOWN_GOOD };
    const meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load(source, opts, discard);
    REQUIRE(loaded.has_value());
    std::ostringstream out;
    const meios::emit_result result = meios::flatten(loaded->robot, out, discard);
    REQUIRE(result.status == meios::emit_status::ok);
    return out.str();
}

}

TEST_CASE("flatten output is byte-identical to the committed baseline",
          "[urdf][flatten][corpus]")
{
    int cases = 0;

    const std::filesystem::path baseline =
        std::filesystem::path{ MEIOS_FLATTEN_GOLDEN_DIR } / "lbr_iiwa_14_r820.flat.urdf";

    const std::string produced = flatten_known_good();

    // Regeneration hatch: MEIOS_FLATTEN_BLESS=1 rewrites the baseline from the
    // current flatten output, so a legitimate upstream-pin bump has one documented,
    // auditable command instead of a hand-edited golden. Off in every normal run,
    // which therefore only ever compares.
    if(const char *bless = std::getenv("MEIOS_FLATTEN_BLESS"); bless != nullptr && bless[0] == '1')
    {
        std::ofstream out(baseline, std::ios::binary | std::ios::trunc);
        out << produced;
    }

    INFO("baseline: " << baseline.string());
    REQUIRE(std::filesystem::exists(baseline));
    REQUIRE(produced == slurp(baseline));
    ++cases;

    REQUIRE(cases == 1);
}
