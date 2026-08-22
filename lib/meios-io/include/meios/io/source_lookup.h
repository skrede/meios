#ifndef HPP_GUARD_MEIOS_IO_SOURCE_LOOKUP_H
#define HPP_GUARD_MEIOS_IO_SOURCE_LOOKUP_H

#include "meios/io/resolved_asset.h"

#include "meios/expected.h"

#include "meios/diagnostic/operation_failure.h"

#include <optional>

namespace meios
{

using source_lookup_result = expected<std::optional<resolved_asset>, operation_failure>;

}

#endif
