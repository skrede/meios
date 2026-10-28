#ifndef HPP_GUARD_MEIOS_TESTS_DIFFERENTIAL_DIVERGENCE_H
#define HPP_GUARD_MEIOS_TESTS_DIFFERENTIAL_DIVERGENCE_H

#include "differential_xml.h"
#include "differential_seed.h"
#include "differential_document.h"

#include "../refusal_causes.h"
#include "../differential_verdict.h"

#include <meios/xacro.h>

#include <catch2/catch_test_macros.hpp>

#include <tuple>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>

namespace differential
{

#ifdef MEIOS_TEST_HAS_YAML

using verdicts = std::vector<std::tuple<std::string, std::string, std::string>>;

inline std::string upstream_render(const std::string &renders_dir, const std::string &id)
{
    const std::filesystem::path scratch = render_path(renders_dir, id, ".txt");
    REQUIRE(std::filesystem::exists(scratch));
    return trimmed(slurp(scratch));
}

// A refusing render carries upstream's own wording after the verdict, which no manifest column can
// hold: a tab is the column separator. Both sides therefore record the verdict, and the wording of
// a refusal is what expressions.cases exists to carry.
inline std::string upstream_value(const std::string &rendered)
{
    return refused(rendered) ? std::string("REFUSED") : rendered;
}

// A whole document is carried as the project's own canonical text, so attribute order and
// insignificant whitespace are not a difference and the row stays one line. A refusal is carried as
// the code this side gives for upstream's cause, so two refusals agree only on the same cause.
inline std::string upstream_document(const std::string &rendered)
{
    if(!refused(rendered))
        return meios::canonical_xml(rendered);
    const upstream_refusal cause                     = parse_refusal(rendered);
    const std::optional<meios::diagnostic_code> code = mapped_cause(cause.outer, cause.inner, cause.line);
    if(!code)
        FAIL("unmapped upstream refusal: " << cause.outer << ' ' << cause.inner << ": " << cause.line);
    return "REFUSED " + std::string(meios::to_string(*code));
}

inline void record_divergence(const std::vector<oracle::row> &manifest, const std::string &id,
                              const std::string &upstream_text, const std::string &meios_value,
                              verdicts &observed)
{
    REQUIRE(upstream_text != meios_value);
    std::string detail;
    if(!matches_manifest(manifest, id, upstream_text, meios_value, detail))
        FAIL(detail);
    observed.emplace_back(id, upstream_text, meios_value);
}

inline void compare_divergence_probe(const probe &one, const std::string &renders_dir, const std::vector<oracle::row> &manifest, verdicts &observed)
{
    const std::string id(one.id);
    INFO("probe " << id << ": " << one.expression);
    const std::string rendered = upstream_render(renders_dir, id);
    record_divergence(manifest, id, upstream_value(rendered), observed_value(one), observed);
}

inline void compare_document_probe(const document_probe &one, const std::string &renders_dir, const std::vector<oracle::row> &manifest, verdicts &observed)
{
    const std::string id(one.id);
    INFO("document probe " << id);
    const std::string rendered = upstream_render(renders_dir, id);
    record_divergence(manifest, id, upstream_document(rendered), observed_document(one), observed);
}

#endif

}

#endif
