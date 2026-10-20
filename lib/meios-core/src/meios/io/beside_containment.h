#ifndef HPP_GUARD_MEIOS_CORE_IO_BESIDE_CONTAINMENT_H
#define HPP_GUARD_MEIOS_CORE_IO_BESIDE_CONTAINMENT_H

#include "meios/io/resolved_asset.h"

#include "meios/expected.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/operation_failure.h"

#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail
{

// The root is canonical and the request is the canonical target relative to it, so a reader that
// follows no link below its root opens exactly the file the decision admitted.
struct contained_read
{
    std::filesystem::path real;
    std::filesystem::path root;
};

using contained_read_result = expected<std::optional<contained_read>, operation_failure>;

contained_read_result contain_under(const std::filesystem::path &root, const std::filesystem::path &candidate);

// The package lookup the path-backed sources share: an escape or a canonicalize failure is logged as
// uncontained, and a hit carries the canonical root and the canonical target relative to it.
std::optional<resolved_asset> locate_contained(const std::filesystem::path &root, std::string_view package, std::string_view relative, log_sink &log);

contained_read_result contain_beside(const std::filesystem::path &document, const std::filesystem::path &spec);

// Names the join only where the spelling stays inside the directory the document was named in, so an
// escape the containment decision found for it can only have been made by a link.
std::optional<std::filesystem::path> beside_link(const std::filesystem::path &document, const std::filesystem::path &spec);

}

#endif
