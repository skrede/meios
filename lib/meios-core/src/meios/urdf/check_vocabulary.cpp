#include "urdf_detail.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <span>
#include <string>
#include <algorithm>
#include <string_view>

namespace meios::detail
{

namespace
{

constexpr std::string_view supported_version = "1.0";

constexpr std::string_view extension_elements[] = { "gazebo", "ros2_control", "transmission",
                                                    "sensor" };

constexpr std::string_view robot_children[] = { "material", "link", "joint" };
constexpr std::string_view robot_attributes[] = { "name", "version" };
constexpr std::string_view material_children[] = { "color", "texture" };
constexpr std::string_view name_attribute[] = { "name" };
constexpr std::string_view color_attributes[] = { "rgba" };
constexpr std::string_view filename_attribute[] = { "filename" };
constexpr std::string_view link_children[] = { "inertial", "visual", "collision" };
constexpr std::string_view inertial_children[] = { "origin", "mass", "inertia" };
constexpr std::string_view mass_attributes[] = { "value" };
constexpr std::string_view inertia_attributes[] = { "ixx", "ixy", "ixz", "iyy", "iyz", "izz" };
constexpr std::string_view origin_attributes[] = { "xyz", "rpy" };
constexpr std::string_view visual_children[] = { "origin", "geometry", "material" };
constexpr std::string_view collision_children[] = { "origin", "geometry" };
constexpr std::string_view geometry_children[] = { "box", "cylinder", "sphere", "mesh" };
constexpr std::string_view box_attributes[] = { "size" };
constexpr std::string_view cylinder_attributes[] = { "radius", "length" };
constexpr std::string_view sphere_attributes[] = { "radius" };
constexpr std::string_view mesh_attributes[] = { "filename", "scale" };
constexpr std::string_view joint_children[] = { "parent", "child",    "origin", "axis",
                                                "limit",  "mimic",    "dynamics",
                                                "safety_controller",  "calibration" };
constexpr std::string_view joint_attributes[] = { "name", "type" };
constexpr std::string_view link_attribute[] = { "link" };
constexpr std::string_view axis_attributes[] = { "xyz" };
constexpr std::string_view limit_attributes[] = { "lower", "upper", "effort", "velocity" };
constexpr std::string_view mimic_attributes[] = { "joint", "multiplier", "offset" };
constexpr std::string_view dynamics_attributes[] = { "damping", "friction" };
constexpr std::string_view safety_attributes[] = { "soft_lower_limit", "soft_upper_limit",
                                                   "k_position", "k_velocity" };
constexpr std::string_view calibration_attributes[] = { "rising", "falling" };

struct entry
{
    std::string_view                  element;
    std::span<const std::string_view> children;
    std::span<const std::string_view> attributes;
};

constexpr entry vocabulary[] = {
    { "robot",             robot_children,     robot_attributes },
    { "material",          material_children,  name_attribute },
    { "color",             {},                 color_attributes },
    { "texture",           {},                 filename_attribute },
    { "link",              link_children,      name_attribute },
    { "inertial",          inertial_children,  {} },
    { "mass",              {},                 mass_attributes },
    { "inertia",           {},                 inertia_attributes },
    { "origin",            {},                 origin_attributes },
    { "visual",            visual_children,    name_attribute },
    { "collision",         collision_children, name_attribute },
    { "geometry",          geometry_children,  {} },
    { "box",               {},                 box_attributes },
    { "cylinder",          {},                 cylinder_attributes },
    { "sphere",            {},                 sphere_attributes },
    { "mesh",              {},                 mesh_attributes },
    { "joint",             joint_children,     joint_attributes },
    { "parent",            {},                 link_attribute },
    { "child",             {},                 link_attribute },
    { "axis",              {},                 axis_attributes },
    { "limit",             {},                 limit_attributes },
    { "mimic",             {},                 mimic_attributes },
    { "dynamics",          {},                 dynamics_attributes },
    { "safety_controller", {},                 safety_attributes },
    { "calibration",       {},                 calibration_attributes },
};

bool contains(std::span<const std::string_view> names, std::string_view name)
{
    return std::find(names.begin(), names.end(), name) != names.end();
}

const entry *lookup(std::string_view element)
{
    for(const entry &known : vocabulary)
        if(known.element == element)
            return &known;
    return nullptr;
}

std::string named(pugi::xml_node element)
{
    std::string out = element.name();
    if(pugi::xml_attribute name = element.attribute("name"))
        out += " name='" + std::string(name.value()) + '\'';
    return out;
}

void check_children(pugi::xml_node element, const entry &known, std::string_view text,
                    const std::filesystem::path &file, parse_context &ctx)
{
    for(pugi::xml_node child : element.children())
    {
        if(child.type() != pugi::node_element || contains(known.children, child.name()))
            continue;
        const bool blessed = contains(extension_elements, child.name());
        ctx.log.log(level::warn,
                    blessed ? diagnostic_code::extension_ignored : diagnostic_code::unknown_element,
                    node_location(child, text, file),
                    std::string("dropping ") + (blessed ? "extension" : "unrecognized")
                        + " element <" + named(child) + "> under <" + element.name() + '>');
    }
}

// An xmlns declaration is XML infrastructure rather than description vocabulary, and a
// document reaching the reader unexpanded still carries the xacro one the emitter strips.
bool is_namespace_declaration(std::string_view attribute)
{
    return attribute.substr(0, 5) == "xmlns";
}

void check_attributes(pugi::xml_node element, const entry &known, std::string_view text,
                      const std::filesystem::path &file, parse_context &ctx)
{
    for(pugi::xml_attribute attr : element.attributes())
        if(!contains(known.attributes, attr.name()) && !is_namespace_declaration(attr.name()))
            ctx.log.log(level::warn, diagnostic_code::unknown_attribute,
                        node_location(element, text, file),
                        "dropping unrecognized attribute '" + std::string(attr.name()) + "' on <"
                            + element.name() + '>');
}

bool check_version(pugi::xml_node robot, std::string_view text, const std::filesystem::path &file,
                   parse_context &ctx)
{
    const pugi::xml_attribute declared = robot.attribute("version");
    if(!declared || declared.value() == supported_version)
        return true;
    bool ok = true;
    report_structural(ctx, node_location(robot, text, file), diagnostic_code::unsupported_version,
                      "robot declares version '" + std::string(declared.value())
                          + "', and this library implements only version "
                          + std::string(supported_version),
                      ok);
    return ok;
}

}

bool vocabulary_governs(std::string_view parent, std::string_view element)
{
    const entry *known = lookup(parent);
    return known != nullptr && contains(known->children, element);
}

bool check_vocabulary(pugi::xml_node element, std::string_view text,
                      const std::filesystem::path &file, parse_context &ctx)
{
    const entry *known = lookup(element.name());
    if(known == nullptr)
        return true;
    check_attributes(element, *known, text, file, ctx);
    check_children(element, *known, text, file, ctx);
    if(element.name() != std::string_view("robot"))
        return true;
    return check_version(element, text, file, ctx);
}

}
