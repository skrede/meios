#ifndef HPP_GUARD_MEIOS_TESTS_DIFFERENTIAL_DOCUMENT_H
#define HPP_GUARD_MEIOS_TESTS_DIFFERENTIAL_DOCUMENT_H

#include "differential_xml.h"
#include "differential_seed.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>
#include <meios/io/source_handle.h>
#include <meios/io/directory_source.h>

#include <array>
#include <memory>
#include <string>
#include <optional>
#include <filesystem>
#include <string_view>
#include <system_error>

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
    bool binds_directory;
    bool serves_directory;
    std::string_view relative_root;
    bool root_under_directory;
};

// Spelled exactly as differential.py's own tuple spells them, so an id names the same fixture and
// the same rendered file whichever side wrote it.
inline constexpr std::array<document_probe, 15> document_probes{
        document_probe{true, "non_text_mapping_key"},      document_probe{false, "argument_without_default"},
        document_probe{false, "argument_helper_call"},     document_probe{false, "argument_above_declaration"},
        document_probe{false, "include_parent_directory"}, document_probe{false, "include_absolute_path", true},
        document_probe{false, "include_sole_block"},       document_probe{false, "dirname_command"},
        document_probe{false, "include_outside_link"},     document_probe{false, "include_package_filename", false, true},
        document_probe{false, "find_relative_root", false, false, "find_relative_root.d"},
        document_probe{false, "find_dotted_root", false, false, "./find_dotted_root.d", true},
        document_probe{false, "property_forward_reference"},
        document_probe{false, "include_find_climbs_out_of_package", false, false, "include_find_climbs_out_of_package.d", true},
        document_probe{false, "include_glob_filename", false, false, "include_glob_filename.d", true},
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
    meios::text_resource_result fetch(std::string_view spec, const std::filesystem::path &) override
    {
        const std::filesystem::path named = std::filesystem::path{ MEIOS_XACRO_PROBE_DIR } / spec;
        if(!std::filesystem::exists(named))
            return meios::unexpected<meios::text_resource_failure>(meios::text_resource_failure::unreachable);
        return slurp(named);
    }
};

// A committed fixture cannot spell a machine's absolute path, so a directory-bound probe is handed
// the directory this side reads it from, as differential.py hands upstream the one it stages it in.
inline meios::eval_scope document_scope(const document_probe &one)
{
    meios::eval_scope scope;
    if(one.binds_directory && !std::string_view{MEIOS_XACRO_PROBE_DIR}.empty())
        scope.bind_argument("probe_directory", meios::value{std::string(MEIOS_XACRO_PROBE_DIR)});
    if(!one.reads_documents)
        return scope;
    scope.install_text_loader(meios::text_resource_loader{ std::make_unique<probe_loader>() });
    scope.install_yaml_parser(meios::make_yaml_parser());
    return scope;
}

// A root is handed from the probe directory as differential.py hands upstream the same spelling: the
// relative root as written, and the dotted root or a probe's own package tree joined under that directory.
class probe_working_directory
{
public:
    probe_working_directory()
            : m_saved(std::filesystem::current_path())
    {
        std::filesystem::current_path(MEIOS_XACRO_PROBE_DIR);
    }

    ~probe_working_directory()
    {
        std::error_code ignored;
        std::filesystem::current_path(m_saved, ignored);
    }

    probe_working_directory(const probe_working_directory &)            = delete;
    probe_working_directory &operator=(const probe_working_directory &) = delete;

private:
    std::filesystem::path m_saved;
};

// This side's verdict for a whole document: the expansion entry the load path runs, reduced to the
// canonical text of what it rendered or to the bare refusal a manifest column can hold. It is named
// after the fixture, so a relative include in it resolves beside the fixture as upstream's does.
inline std::string observed_document(const document_probe &one)
{
    meios::log_sink silent;
    std::optional<probe_working_directory> here;
    meios::source_stack sources;
    if(!one.relative_root.empty() && !std::string_view{MEIOS_XACRO_PROBE_DIR}.empty())
    {
        here.emplace();
        const std::filesystem::path written{one.relative_root};
        const std::filesystem::path root = one.root_under_directory ? std::filesystem::path{MEIOS_XACRO_PROBE_DIR} / written : written;
        sources.push_back(meios::source_handle{meios::directory_source{root, silent}});
    }
    if(one.serves_directory && !std::string_view{MEIOS_XACRO_PROBE_DIR}.empty())
        sources.push_back(meios::source_handle{meios::directory_source{MEIOS_XACRO_PROBE_DIR, silent}});
    meios::eval_scope scope = document_scope(one);
    const std::filesystem::path fixture = probe_file(one.id, ".xacro");
    const std::string source = slurp(fixture);
    const meios::expected<meios::expansion, meios::expansion_error> rendered = meios::expand(
        source, scope, sources, fixture, meios::expansion_limits{}, silent);
    return rendered.has_value() ? meios::canonical_xml(rendered->document) : std::string("REFUSED");
}

#endif

}

#endif
