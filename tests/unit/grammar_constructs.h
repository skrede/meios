#ifndef HPP_GUARD_MEIOS_UNIT_GRAMMAR_CONSTRUCTS_H
#define HPP_GUARD_MEIOS_UNIT_GRAMMAR_CONSTRUCTS_H

#include <string_view>

namespace grammar
{

enum class carrier
{
    agreement,
    corpus,
    manifest,
    generated,
    expression,
    unit,
    boundary,
    unreachable,
};

enum class standing
{
    agrees,
    refuses_listed,
    open,
};

// Paired both ways against the derived record by the stem beside this header, so a construct with
// no pairing and a pairing with no construct both fail. Keys are separated by spaces, except that a
// unit key is one whole test name, spaces and all; a boundary pairing's key is the scope it leaves
// unmeasured, and its standing is open because nothing here measures it. A unit case pins only this
// side, so a unit pairing claims agrees: upstream warns where this side does not, on a document both
// render alike. A manifest key may name a document upstream never reaches the construct in, where
// this side reaching it is the recorded difference.
struct pairing
{
    std::string_view construct;
    carrier by;
    std::string_view keys;
    standing now;
};

// A grammar site of the record no document reaches under the pinned install. Its carrier is the
// evidence, and its standing stays open because nothing measures it.
struct unreached_site
{
    std::string_view construct;
    std::string_view evidence;
};

}

#endif
