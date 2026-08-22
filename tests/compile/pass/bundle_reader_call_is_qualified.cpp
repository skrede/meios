// meios::detail declares two-argument readers of the same two names in the core-private header
// below, and unqualified lookup stops at the first enclosing namespace that declares the name at
// all. Seeing that header first is therefore what turns an unqualified call in the bundle header
// into a compile error, and only this include order can express the claim.
#include "meios/io/text_reader_operations.h"

#include <meios/io/resolved_asset.h>
#include <meios/bundle/asset_bytes.h>

#include <filesystem>

meios::text_read_result read_through_bundle_header(const std::filesystem::path &path)
{
    return meios::detail::read_asset_bytes(meios::resolved_asset{path});
}
