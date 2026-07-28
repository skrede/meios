#ifndef HPP_GUARD_MEIOS_URDF_ASSET_REPORT_H
#define HPP_GUARD_MEIOS_URDF_ASSET_REPORT_H

#include "meios/urdf/parse_context.h"

#include "meios/diagnostic/source_location.h"

#include <string>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

// A source answers where a thing is and never what kind of thing it is, so the arm that reaches
// the filesystem through a source is put to this question exactly as the arms that reach it
// through a root are. A directory, a device and a broken link are deliberately not told apart:
// what a consumer asks is whether there is an asset there, and all three answer no.
std::optional<std::string> asset_or_missing(const std::filesystem::path &real, parse_context &ctx,
                                            const source_location &loc, const std::string &uri);

// Grades an absent asset by the missing-asset policy, and records what a silenced diagnostic
// would have withdrawn, so the claim answers for the document rather than for the log.
void report_missing(parse_context &ctx, const source_location &loc, const std::string &uri);

// The three below refuse at error level whatever the missing-asset policy says. Separate named
// functions rather than a flag on the policy-honoring one, so a call site reads as a refusal.
void report_unreachable(parse_context &ctx, const source_location &loc, const std::string &uri);

void report_foreign_scheme(parse_context &ctx, const source_location &loc, const std::string &uri,
                           std::string_view scheme);

void report_foreign_authority(parse_context &ctx, const source_location &loc,
                              const std::string &uri, std::string_view authority);

}

#endif
