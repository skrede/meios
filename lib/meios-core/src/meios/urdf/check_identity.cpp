#include "urdf_detail.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <set>
#include <string>
#include <cctype>
#include <algorithm>
#include <string_view>

namespace meios::detail
{

namespace
{

using name_set = std::set<std::string>;

bool blank(std::string_view text)
{
    return std::all_of(text.begin(), text.end(),
                       [](unsigned char c) { return std::isspace(c) != 0; });
}

bool has_name(pugi::xml_node node, std::string_view text, const std::filesystem::path &file,
              parse_context &ctx, bool &ok)
{
    const pugi::xml_attribute name = node.attribute("name");
    if(name && !blank(name.value()))
        return true;
    report_structural(ctx, node_location(node, text, file), diagnostic_code::empty_name,
                      std::string("<") + node.name() + "> carries no name", ok);
    return false;
}

// Names are compared as the bytes the author wrote: two names differing in case or in
// surrounding whitespace are two names, because any folding would invent an equivalence
// the document never stated.
name_set collect_names(pugi::xml_node robot, const char *kind, std::string_view text,
                       const std::filesystem::path &file, parse_context &ctx, bool &ok)
{
    name_set names;
    for(pugi::xml_node node : robot.children(kind))
    {
        if(!has_name(node, text, file, ctx, ok))
            continue;
        const std::string name = node.attribute("name").value();
        if(!names.insert(name).second)
            report_structural(ctx, node_location(node, text, file),
                              diagnostic_code::duplicate_name,
                              std::string(kind) + " name '" + name + "' is declared twice", ok);
    }
    return names;
}

// An absent <parent> element and a <parent> without a link attribute both yield the empty
// string downstream, so both are refused here rather than arriving as a dangling reference.
void check_link_ref(pugi::xml_node edge, const char *side, const name_set &links,
                    std::string_view text, const std::filesystem::path &file, parse_context &ctx,
                    bool &ok)
{
    const std::string owner = edge.attribute("name").value();
    const pugi::xml_node end = edge.child(side);
    const pugi::xml_attribute ref = end.attribute("link");
    if(!ref || blank(ref.value()))
    {
        report_structural(ctx, node_location(end ? end : edge, text, file),
                          diagnostic_code::empty_name,
                          "joint '" + owner + "' names no " + side + " link", ok);
        return;
    }
    if(links.count(ref.value()) == 0)
        report_structural(ctx, node_location(end, text, file), diagnostic_code::undeclared_link,
                          "joint '" + owner + "' names undeclared " + side + " link '"
                              + ref.value() + "'",
                          ok);
}

void check_mimic(pugi::xml_node edge, const name_set &joints, std::string_view text,
                 const std::filesystem::path &file, parse_context &ctx, bool &ok)
{
    const pugi::xml_node source = edge.child("mimic");
    if(!source)
        return;
    const std::string owner = edge.attribute("name").value();
    const std::string target = source.attribute("joint").value();
    if(target == owner)
        report_structural(ctx, node_location(source, text, file), diagnostic_code::dangling_mimic,
                          "joint '" + owner + "' mimics itself", ok);
    else if(joints.count(target) == 0)
        report_structural(ctx, node_location(source, text, file), diagnostic_code::dangling_mimic,
                          "joint '" + owner + "' mimics undeclared joint '" + target + "'", ok);
}

// A <material> under a <visual> that carries no name and defines neither a color nor a
// texture references nothing and declares nothing, so there is no material to read from it.
void check_visual_materials(pugi::xml_node node, std::string_view text,
                            const std::filesystem::path &file, parse_context &ctx, bool &ok)
{
    for(pugi::xml_node vis : node.children("visual"))
        for(pugi::xml_node mat : vis.children("material"))
        {
            if(mat.child("color") || mat.child("texture"))
                continue;
            const pugi::xml_attribute name = mat.attribute("name");
            if(!name || blank(name.value()))
                report_structural(ctx, node_location(mat, text, file), diagnostic_code::empty_name,
                                  "<material> under a <visual> names no material", ok);
        }
}

}

bool check_identity(pugi::xml_node robot, std::string_view text, const std::filesystem::path &file,
                    parse_context &ctx)
{
    bool ok = true;
    const name_set links = collect_names(robot, "link", text, file, ctx, ok);
    const name_set joints = collect_names(robot, "joint", text, file, ctx, ok);
    collect_names(robot, "material", text, file, ctx, ok);
    if(!robot.child("link"))
        report_structural(ctx, node_location(robot, text, file), diagnostic_code::no_links,
                          "robot '" + std::string(robot.attribute("name").value())
                              + "' declares no links",
                          ok);
    for(pugi::xml_node edge : robot.children("joint"))
    {
        check_link_ref(edge, "parent", links, text, file, ctx, ok);
        check_link_ref(edge, "child", links, text, file, ctx, ok);
        check_mimic(edge, joints, text, file, ctx, ok);
    }
    for(pugi::xml_node node : robot.children("link"))
        check_visual_materials(node, text, file, ctx, ok);
    return ok;
}

}
