#ifndef HPP_GUARD_MEIOS_TESTS_DIFFERENTIAL_XML_H
#define HPP_GUARD_MEIOS_TESTS_DIFFERENTIAL_XML_H

#include <pugixml.hpp>

#include <cctype>
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <sstream>
#include <filesystem>
#include <string_view>

namespace differential
{

inline std::string slurp(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// Mirrors differential.py's own sanitize(): a scratch filename safe on every platform, built
// identically on both sides so an id names the same file regardless of which side wrote it.
inline std::string sanitize(const std::string &case_id)
{
    std::string out;
    for(char c : case_id)
        out.push_back(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_'
                              || c == '-'
                          ? c
                          : '_');
    return out;
}

inline std::filesystem::path render_path(const std::string &renders_dir, const std::string &id,
                                         const char *suffix)
{
    return std::filesystem::path{ renders_dir } / (sanitize(id) + suffix);
}

inline bool refused(const std::string &text) { return text.rfind("REFUSED", 0) == 0; }

// The attribute value a tiny <e v="..."/> fragment carries, read back for the divergence
// manifest's exact-text match -- the manifest compares plain values, never XML shape.
inline std::string attribute_v(const std::string &xml_text)
{
    pugi::xml_document doc;
    doc.load_buffer(xml_text.data(), xml_text.size());
    return doc.first_child().attribute("v").value();
}

inline std::string escaped_fragment(const std::string &value)
{
    std::string out = "<e v=\"";
    for(char c : value)
    {
        if(c == '&')      out += "&amp;";
        else if(c == '"') out += "&quot;";
        else if(c == '<') out += "&lt;";
        else              out.push_back(c);
    }
    return out + "\"/>";
}

inline bool numeric_text(const std::string &text, double &value)
{
    char *end = nullptr;
    value = std::strtod(text.c_str(), &end);
    return end != text.c_str() && *end == '\0';
}

// Exactly, not within a tolerance, mirroring model_facts.h's same_number(): attribute values are
// read as numbers where the text reads as a number, as text otherwise (D-08).
inline bool same_scalar(const std::string &a, const std::string &b)
{
    double na = 0.0;
    double nb = 0.0;
    if(numeric_text(a, na) && numeric_text(b, nb))
        return na == nb;
    return a == b;
}

inline bool same_children(pugi::xml_node a, pugi::xml_node b, std::string &detail);

inline bool same_attributes(pugi::xml_node a, pugi::xml_node b, std::string &detail)
{
    const std::vector<pugi::xml_attribute> aa(a.attributes_begin(), a.attributes_end());
    const std::vector<pugi::xml_attribute> ab(b.attributes_begin(), b.attributes_end());
    if(aa.size() != ab.size())
    {
        detail = "attribute count differs on <" + std::string(a.name()) + ">";
        return false;
    }
    for(std::size_t at = 0; at < aa.size(); ++at)
    {
        const bool same = std::string_view(aa[at].name()) == std::string_view(ab[at].name())
                        && same_scalar(aa[at].value(), ab[at].value());
        if(!same)
        {
            detail = "attribute '" + std::string(aa[at].name()) + "' on <" + a.name()
                   + "> differs";
            return false;
        }
    }
    return true;
}

inline bool same_element(pugi::xml_node a, pugi::xml_node b, std::string &detail)
{
    if(std::string_view(a.name()) != std::string_view(b.name()))
    {
        detail = "element name differs";
        return false;
    }
    return same_attributes(a, b, detail) && same_children(a, b, detail);
}

inline bool same_children(pugi::xml_node a, pugi::xml_node b, std::string &detail)
{
    const std::vector<pugi::xml_node> ca(a.children().begin(), a.children().end());
    const std::vector<pugi::xml_node> cb(b.children().begin(), b.children().end());
    if(ca.size() != cb.size())
    {
        detail = "child count differs under <" + std::string(a.name()) + ">";
        return false;
    }
    for(std::size_t at = 0; at < ca.size(); ++at)
    {
        const bool same = ca[at].type() == cb[at].type()
                        && (ca[at].type() == pugi::node_pcdata
                                ? same_scalar(ca[at].value(), cb[at].value())
                                : same_element(ca[at], cb[at], detail));
        if(!same)
            return false;
    }
    return true;
}

// The numeric-aware attribute comparator D-08 requires, layered on canonical_xml()'s own
// output rather than a second normalizer (D-07): two canonical_xml() strings in, structurally
// compared with attribute values read as numbers where the text reads as a number.
inline bool canonical_matches(const std::string &upstream_canon, const std::string &meios_canon,
                              std::string &detail)
{
    pugi::xml_document a;
    pugi::xml_document b;
    a.load_buffer(upstream_canon.data(), upstream_canon.size());
    b.load_buffer(meios_canon.data(), meios_canon.size());
    return same_children(a, b, detail);
}

}

#endif
