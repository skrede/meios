#ifndef HPP_GUARD_MEIOS_URDF_ASSET_REPORT_H
#define HPP_GUARD_MEIOS_URDF_ASSET_REPORT_H

#include "meios/urdf/parse_context.h"

#include "meios/diagnostic/source_location.h"

#include <string>
#include <string_view>

namespace meios::detail
{

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
