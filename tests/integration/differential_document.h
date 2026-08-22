#ifndef HPP_GUARD_MEIOS_TESTS_DIFFERENTIAL_DOCUMENT_H
#define HPP_GUARD_MEIOS_TESTS_DIFFERENTIAL_DOCUMENT_H

#include "differential_xml.h"
#include "differential_seed.h"

#include "../differential_verdict.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <tuple>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <string_view>

#ifndef MEIOS_XACRO_PROBE_DIR
    #define MEIOS_XACRO_PROBE_DIR ""
#endif

namespace differential
{

#ifdef MEIOS_TEST_HAS_YAML

// A construct needing a macro, a conditional and a call site cannot be written as an expression
// span, so its probe is a whole document read from a fixture both sides open by the same id --
// the document itself is the text neither side repeats.
struct document_probe
{
    bool reads_documents;
    std::string_view id;
};

// Spelled exactly as differential.py's own tuple spells them, so an id names the same fixture and
// the same rendered file whichever side wrote it.
inline constexpr std::array<document_probe, 1> document_probes{
    document_probe{ true, "non_text_mapping_key" }
};

inline std::filesystem::path probe_file(std::string_view id, const char *suffix)
{
    return std::filesystem::path{ MEIOS_XACRO_PROBE_DIR } / (std::string(id) + suffix);
}

// Upstream resolves an auxiliary document against the document that reads it, which for a probe
// is the fixture directory; serving the same directory here keeps the pair self-contained.
class probe_loader final : public meios::text_resource_loader::fetcher
{
public:
    std::optional<std::string> fetch(std::string_view spec, const std::filesystem::path &) override
    {
        const std::filesystem::path named = std::filesystem::path{ MEIOS_XACRO_PROBE_DIR } / spec;
        if(!std::filesystem::exists(named))
            return std::nullopt;
        return slurp(named);
    }
};

inline meios::eval_scope document_scope(const document_probe &one)
{
    meios::eval_scope scope;
    if(!one.reads_documents)
        return scope;
    scope.install_text_loader(meios::text_resource_loader{ std::make_unique<probe_loader>() });
    scope.install_yaml_parser(meios::make_yaml_parser());
    return scope;
}

// This side's verdict for a whole document: the expansion entry the load path runs, reduced to the
// canonical text of what it rendered or to the bare refusal a manifest column can hold.
inline std::string observed_document(const document_probe &one)
{
    meios::log_sink silent;
    meios::source_stack sources;
    meios::eval_scope scope = document_scope(one);
    const std::string source = slurp(probe_file(one.id, ".xacro"));
    const meios::expected<meios::expansion, meios::expansion_error> rendered = meios::expand(
        source, scope, sources, "probe.xacro", meios::expansion_limits{}, silent);
    return rendered.has_value() ? meios::canonical_xml(rendered->document) : std::string("REFUSED");
}

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
// insignificant whitespace are not a difference and the row stays one line.
inline std::string upstream_document(const std::string &rendered)
{
    return refused(rendered) ? std::string("REFUSED") : meios::canonical_xml(rendered);
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

#endif

}

#endif
