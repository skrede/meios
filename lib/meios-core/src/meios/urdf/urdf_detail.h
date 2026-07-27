#ifndef HPP_GUARD_MEIOS_URDF_URDF_DETAIL_H
#define HPP_GUARD_MEIOS_URDF_URDF_DETAIL_H

#include "meios/urdf/parse_context.h"

#include "meios/records/link.h"
#include "meios/records/joint.h"
#include "meios/records/geometry.h"
#include "meios/records/material.h"

#include "meios/math/vector3.h"
#include "meios/math/transform.h"

#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"

#include "meios/detail/text_location.h"

#include <pugixml.hpp>

#include <string>
#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>
#include <unordered_map>

namespace meios::detail
{

using material_table = std::unordered_map<std::string, material<double>>;

bool read_finite(std::string_view text, double &out, parse_context &ctx,
                 const source_location &loc, std::string_view field);

// An attribute the profile marks optional: an absent one keeps the caller's default and is
// not a violation, so a documented default survives; a present one that is not a finite
// number is reported and refused.
bool read_optional(pugi::xml_node node, const char *field, double &out, parse_context &ctx,
                   const source_location &loc);

// An attribute the profile marks required: absence and unreadable text are both refused,
// and the caller drops the element rather than completing it with a value nothing stated.
bool read_required(pugi::xml_node node, const char *field, double &out, parse_context &ctx,
                   const source_location &loc);

// Fills out[0..count) only when the text carries exactly count whitespace-separated
// tokens; an empty or all-whitespace text leaves the caller's defaults and is not a
// violation, so an absent attribute keeps the default the profile documents.
bool read_scalars(std::string_view text, double *out, std::size_t count, parse_context &ctx,
                  const source_location &loc, std::string_view field);

// Leaves out at whatever the caller seeded and answers false only when a present attribute
// could not be read, which is the caller's cue to drop the element the vector belongs to.
bool read_vec3(std::string_view text, vector3<double> &out, parse_context &ctx,
               const source_location &loc, std::string_view field);

// Leaves out at the identity the wiki states for an absent origin and answers false only
// when a present attribute could not be read, which is the caller's cue to drop.
bool read_transform(pugi::xml_node origin, transform<double> &out, parse_context &ctx,
                    const source_location &loc);

// Answers empty for a material whose color the reader refused: the containing <material>
// is dropped rather than carried with a color the document never wrote.
std::optional<material<double>> extract_material(pugi::xml_node node, std::string_view text,
                                                 const std::filesystem::path &file,
                                                 parse_context &ctx);

link<double> extract_link(pugi::xml_node node, std::string_view text,
                          const std::filesystem::path &file, parse_context &ctx,
                          const material_table &materials);

// Answers empty for a joint whose kind cannot be resolved or whose bounded kind declares no
// usable limit: a joint is structural, so a malformed one is refused rather than completed.
std::optional<joint<double>> extract_joint(pugi::xml_node node, std::string_view text,
                                           const std::filesystem::path &file, parse_context &ctx);

std::optional<inertial<double>> read_inertial(pugi::xml_node node, parse_context &ctx,
                                              const source_location &loc);

// Answers whether the mass and the tensor can describe a rigid body at all. Every value it
// reads must already be finite for the arithmetic to mean anything, so it runs only on an
// inertial the reader accepted whole.
bool check_inertia(const inertial<double> &body, parse_context &ctx, const source_location &loc);

// owner names the element the geometry belongs to, so an absent one is reported against the
// element that required it rather than against a node that is not there.
std::optional<geometry<double>> read_geometry(pugi::xml_node node, std::string_view owner,
                                              parse_context &ctx, const source_location &loc);

std::optional<mimic> read_mimic(pugi::xml_node node, parse_context &ctx,
                                const source_location &loc);

std::optional<dynamics<double>> read_dynamics(pugi::xml_node node, parse_context &ctx,
                                              const source_location &loc);

std::optional<safety_controller<double>> read_safety(pugi::xml_node node, parse_context &ctx,
                                                     const source_location &loc);

std::optional<calibration<double>> read_calibration(pugi::xml_node node, parse_context &ctx,
                                                    const source_location &loc);

void resolve_mesh(mesh<double> &shape, parse_context &ctx, const source_location &loc);

void report(parse_context &ctx, const source_location &loc, diagnostic_code code,
            const std::string &message, bool &ok);

// Reports a violation whose consequence is that the containing element is dropped. The
// policy grades how loudly the violation is said, never whether the element survives, so
// the flag report would clear has nothing to say at such a call site.
void report_drop(parse_context &ctx, const source_location &loc, diagnostic_code code,
                 const std::string &message);

// Refuses at error level and clears ok whatever the document-validity policy says; the
// signature cannot show that it never reads the policy the way report does.
void report_structural(parse_context &ctx, const source_location &loc, diagnostic_code code,
                       const std::string &message, bool &ok);

bool run_strictness(pugi::xml_node document, std::string_view text,
                    const std::filesystem::path &file, parse_context &ctx);

// Answers whether the profile governs the named child of the named parent. An extension
// block and an unrecognized element both carry content this library does not describe, so
// the descent stops judging inside their subtrees rather than naming every node in them.
bool vocabulary_governs(std::string_view parent, std::string_view element);

// Discloses unrecognized children and attributes of one element the profile governs; the
// returned flag is cleared only by a structural refusal, never by a disclosure, because
// dropping unrecognized content is legal at every document-validity setting.
bool check_vocabulary(pugi::xml_node element, std::string_view text,
                      const std::filesystem::path &file, parse_context &ctx);

// Refuses a document whose link, joint or material identity does not hold, or whose joints
// reference a link or a mimic target the document never declared. Every rule it applies is
// structural, so the returned flag is false at every document-validity setting; it runs on
// the pugi node because a record carries no file and line to report against.
bool check_identity(pugi::xml_node robot, std::string_view text,
                    const std::filesystem::path &file, parse_context &ctx);

}

#endif
