#ifdef MEIOS_TEST_HAS_YAML

#include "compatibility_ledger.h"

#include "../corpus_record.h"
#include "../native_expansion.h"

#include "../unit/oracle_records.h"
#include "../unit/construct_probe.h"

#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <cstddef>
#include <optional>

namespace
{

struct observation
{
    meios::construct_set exercised;
    std::string shape;
};

std::string shape_text(std::size_t links, std::size_t joints)
{
    return "links=" + std::to_string(links) + " joints=" + std::to_string(joints);
}

// The construct set comes from the expansion, which is where the evaluation happens; the shape
// comes from the load proper, so the recorded native result is a model rather than a count taken
// off the flattened text.
observation observe(const corpus::document &doc)
{
    const std::optional<meios::expansion> out = corpus::expanded(doc);
    REQUIRE(out.has_value());

    meios::load_options opts;
    opts.on_missing    = meios::missing_asset::warn;
    opts.materials     = meios::material_policy::warn;
    opts.args          = doc.args;
    if(!doc.package_root.empty())
        opts.package_roots.push_back(doc.package_root);
    const meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load(doc.path, opts);
    REQUIRE(loaded.has_value());
    return { out->exercised, shape_text(loaded->robot.links.size(), loaded->robot.joints.size()) };
}

}

TEST_CASE("each pinned entry point exercises and loads to what its ledger row records", "[ledger]")
{
    const std::vector<oracle::row> rows = ledger::rows();
    std::size_t compared = 0;
    for(const corpus::document &doc : corpus::documents(MEIOS_CORPUS_DOCUMENTS))
    {
        if(doc.backend != corpus::evaluator::native)
            continue;
        const std::string key = corpus::document_key(doc);
        INFO("entry point: " << key << " (" << doc.path.string() << ")");
        const std::optional<oracle::row> recorded = oracle::lookup(rows, key);
        REQUIRE(recorded.has_value());
        const observation got = observe(doc);
        CHECK(construct_probe::listed(got.exercised)
              == construct_probe::listed(
                  ledger::set_of(ledger::spellings_in(recorded->fields[ledger::exercised_constructs]))));
        CHECK(got.shape == recorded->fields[ledger::native_result]);
        ++compared;
    }
    CHECK(compared == rows.size());
}

#endif
