#ifndef HPP_GUARD_MEIOS_TESTS_DIFFERENTIAL_DOCUMENT_H
#define HPP_GUARD_MEIOS_TESTS_DIFFERENTIAL_DOCUMENT_H

#include "differential_probe_harness.h"

#include <meios/xacro.h>

#include <pugixml.hpp>

#include <array>
#include <string>
#include <optional>
#include <filesystem>
#include <string_view>

namespace differential
{

#ifdef MEIOS_TEST_HAS_YAML

// Spelled exactly as differential.py's own tuple spells them, so an id names the same fixture and
// the same rendered file whichever side wrote it.
inline const std::array<document_probe, 14> document_probes{
        document_probe{true, "non_text_mapping_key"},      document_probe{false, "argument_without_default"},
        document_probe{false, "argument_helper_call"},     document_probe{false, "argument_above_declaration"},
        document_probe{false, "include_parent_directory"}, document_probe{false, "include_absolute_path", true},
        document_probe{false, "include_sole_block"},       document_probe{false, "dirname_command"},
        document_probe{false, "include_outside_link"},     document_probe{false, "include_package_filename", false, true},
        document_probe{false, "find_relative_root", false, false, "find_relative_root.d"},
        document_probe{false, "find_dotted_root", false, false, "./find_dotted_root.d", true},
        document_probe{false, "include_find_climbs_out_of_package", false, false, "include_find_climbs_out_of_package.d", true},
        document_probe{false, "include_glob_filename", false, false, "include_glob_filename.d", true},
};

inline void list_comments_and_instructions(std::string &listed, pugi::xml_node node)
{
    for(pugi::xml_node child : node.children())
    {
        std::string one;
        if(child.type() == pugi::node_comment)
            one = std::string("<!--") + child.value() + "-->";
        else if(child.type() == pugi::node_pi)
            one = std::string("<?") + child.name() + ' ' + child.value() + "?>";
        if(!one.empty())
            listed += (listed.empty() ? "" : " ") + one;
        list_comments_and_instructions(listed, child);
    }
}

// The canonical text drops comments and processing instructions, so a probe marked to keep them
// is compared on them as well. Only the root's are listed: upstream prepends a banner of its own
// before the root, naming the path of the file it read.
inline std::string comments_and_instructions(std::string_view xml)
{
    pugi::xml_document parsed;
    if(!parsed.load_buffer(xml.data(), xml.size(), pugi::parse_default | pugi::parse_comments | pugi::parse_pi))
        return "unparsed";
    std::string listed;
    list_comments_and_instructions(listed, parsed.document_element());
    return listed;
}

inline std::string document_verdict(const document_probe &one, std::string_view xml)
{
    std::string verdict = meios::canonical_xml(xml);
    if(!one.keeps_comments)
        return verdict;
    const std::string listed = comments_and_instructions(xml);
    return verdict + (listed.empty() ? std::string(" comments:") : " comments: " + listed);
}

// The expansion entry the load path runs. It is named after the fixture, so a relative include in
// it resolves beside the fixture as upstream's does.
inline meios::expected<meios::expansion, meios::expansion_error> expanded_probe(const document_probe &one)
{
    meios::log_sink silent;
    std::optional<probe_working_directory> here;
    if((one.in_stage || !one.relative_root.empty() || !one.roots.empty()) && !std::string_view{ MEIOS_XACRO_PROBE_DIR }.empty())
        here.emplace();
    meios::source_stack sources = probe_sources(one, silent);
    meios::eval_scope scope     = document_scope(one);
    const std::filesystem::path fixture = probe_file(one.id, ".xacro");
    const std::string source = slurp(fixture);
    return meios::expand(source, scope, sources, fixture, meios::expansion_limits{}, silent);
}

// This side's verdict for a whole document: what it rendered, reduced as upstream's render is, or
// the refusal and its code, which a manifest column can hold.
inline std::string observed_document(const document_probe &one)
{
    const meios::expected<meios::expansion, meios::expansion_error> rendered = expanded_probe(one);
    if(!rendered.has_value())
        return "REFUSED " + std::string(meios::to_string(rendered.error().code));
    return document_verdict(one, rendered->document);
}

#endif

}

#endif
