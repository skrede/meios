#ifndef HPP_GUARD_MEIOS_SCAN_COLLADA_SCANNER_H
#define HPP_GUARD_MEIOS_SCAN_COLLADA_SCANNER_H

#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/log_sink.h"

#include <string>
#include <vector>

namespace meios
{

class scanner_registry;

// COLLADA image scanner: walks every <init_from> across the document. COLLADA 1.4
// carries the path in element text, 1.5 wraps it in a <ref> child; each ref is
// returned with a leading file:// stripped and percent-escapes decoded.
class collada_scanner
{
public:
    std::vector<std::string> scan(const resolved_asset &asset, log_sink &log);
};

void register_collada_scanner(scanner_registry &registry);

}

#endif
