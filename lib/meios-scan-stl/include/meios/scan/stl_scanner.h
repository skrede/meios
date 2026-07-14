#ifndef HPP_GUARD_MEIOS_SCAN_STL_SCANNER_H
#define HPP_GUARD_MEIOS_SCAN_STL_SCANNER_H

#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/log_sink.h"

#include <string>
#include <vector>

namespace meios
{

class scanner_registry;

// STL (binary and ASCII) carries no external-reference mechanism, so a scan is a
// type-level no-op: an empty result is the honest "scanned, contributes nothing",
// distinct from an unknown extension's default miss.
class stl_scanner
{
public:
    std::vector<std::string> scan(const resolved_asset &, log_sink &) { return {}; }
};

void register_stl_scanner(scanner_registry &registry);

}

#endif
