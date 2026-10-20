#ifndef HPP_GUARD_MEIOS_CORE_IO_BESIDE_CONTAINMENT_H
#define HPP_GUARD_MEIOS_CORE_IO_BESIDE_CONTAINMENT_H

#include "meios/expected.h"

#include "meios/diagnostic/operation_failure.h"

#include <span>
#include <optional>
#include <filesystem>

namespace meios::detail
{

// The root is canonical and the request is the canonical target relative to it, so a reader that
// follows no link below its root opens exactly the file the decision admitted.
struct contained_read
{
    std::filesystem::path real;
    std::filesystem::path root;
    bool in_named_directory;
};

using contained_read_result = expected<std::optional<contained_read>, operation_failure>;

contained_read_result contain_under(const std::filesystem::path &root, const std::filesystem::path &candidate);

// Admits the target where the document was named, then where it really lives, then under each root in
// order; the first directory containing it answers, and a directory the filesystem could not
// canonicalize yields to the next, its failure answered only when none contains the target. A
// spelling through an element that is not a directory is answered as that status failure. A read
// reports whether the directory the document was named in admitted it, rather than a later stage.
contained_read_result contain_beside(const std::filesystem::path &document, const std::filesystem::path &spec, std::span<const std::filesystem::path> roots = {});

bool through_non_directory(const operation_failure &failure);

// Names the join only where the spelling stays inside the directory the document was named in, so an
// escape the containment decision found for it can only have been made by a link.
std::optional<std::filesystem::path> beside_link(const std::filesystem::path &document, const std::filesystem::path &spec);

// Sees through symlink_status, so it names a link to nothing, which weakly_canonical reports as an
// absent file ([fs.op.weakly.canonical]); a spelling beside_link names no join for passes no link.
expected<std::optional<std::filesystem::path>, operation_failure> first_beside_link(const std::filesystem::path &document, const std::filesystem::path &spec);

}

#endif
