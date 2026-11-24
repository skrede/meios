#ifndef HPP_GUARD_MEIOS_DETAIL_ATTR_VALUE_REPLAY_H
#define HPP_GUARD_MEIOS_DETAIL_ATTR_VALUE_REPLAY_H

#include <string>
#include <vector>
#include <cstddef>
#include <optional>
#include <string_view>

namespace meios::detail
{

struct replayed_value
{
    std::size_t start;
    std::string decoded;
    std::vector<std::size_t> raw_at;
};

// Replays pugixml's default attribute decoding (parse_escapes, parse_eol, parse_wconv_attribute) over the value at
// DOM position `attr_index` of the already-validated start tag at `tag_offset`, matched by position because pugixml
// admits duplicate and namespace-prefixed names. Empty when that value cannot be located in the raw tag.
std::optional<replayed_value> replay_attr_value(std::string_view text, std::size_t tag_offset, std::size_t attr_index);

}

#endif
