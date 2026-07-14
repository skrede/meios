#include "meios/xacro/structural.h"

#include <pugixml.hpp>

#include <string>
#include <vector>
#include <algorithm>
#include <string_view>

namespace meios
{

namespace
{

void collapse(std::string &out, std::string_view text)
{
    bool pending = false;
    for(char c : text)
    {
        if(c == ' ' || c == '\t' || c == '\n' || c == '\r')
        {
            pending = true;
            continue;
        }
        if(pending && !out.empty())
            out.push_back(' ');
        pending = false;
        out.push_back(c);
    }
}

void write_open(std::string &out, pugi::xml_node node)
{
    out.push_back('<');
    out += node.name();
    std::vector<pugi::xml_attribute> attrs(node.attributes_begin(), node.attributes_end());
    std::sort(attrs.begin(), attrs.end(), [](pugi::xml_attribute a, pugi::xml_attribute b) {
        return std::string_view(a.name()) < std::string_view(b.name());
    });
    for(pugi::xml_attribute attr : attrs)
    {
        out.push_back(' ');
        out += attr.name();
        out += "=\"";
        out += attr.value();
        out.push_back('"');
    }
    out.push_back('>');
}

void write_children(std::string &out, pugi::xml_node node)
{
    for(pugi::xml_node child : node.children())
    {
        pugi::xml_node_type kind = child.type();
        if(kind == pugi::node_element)
        {
            write_open(out, child);
            write_children(out, child);
            out += "</";
            out += child.name();
            out.push_back('>');
        }
        else if(kind == pugi::node_pcdata || kind == pugi::node_cdata)
            collapse(out, child.value());
    }
}

}

std::string canonical_xml(std::string_view xml)
{
    pugi::xml_document doc;
    doc.load_buffer(xml.data(), xml.size());
    std::string out;
    write_children(out, doc);
    return out;
}

}
