#ifndef HPP_GUARD_MEIOS_IO_MATERIALIZE_H
#define HPP_GUARD_MEIOS_IO_MATERIALIZE_H

#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/log_sink.h"

namespace meios
{

// Bridges a bytes-only asset to a real filesystem path: drains its bytes into a
// freshly created, uniquely-named temp file that the returned asset owns and
// unlinks on destruction. A path-backed asset is returned unchanged. Every
// materialization emits one info diagnostic, since it is a non-hermetic effect.
resolved_asset materialize(resolved_asset &&asset, log_sink &log);

}

#endif
