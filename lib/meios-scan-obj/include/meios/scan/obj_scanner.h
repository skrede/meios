#ifndef HPP_GUARD_MEIOS_SCAN_OBJ_SCANNER_H
#define HPP_GUARD_MEIOS_SCAN_OBJ_SCANNER_H

#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/log_sink.h"

#include <string>
#include <vector>

namespace meios
{

class scanner_registry;

// Dependency-free Wavefront scanner: a line scan that yields every mtllib library
// and every map_* texture, returning refs verbatim so the closure re-anchors them.
class obj_scanner
{
public:
    std::vector<std::string> scan(const resolved_asset &asset, log_sink &log);
};

void register_obj_scanner(scanner_registry &registry);

}

#endif
