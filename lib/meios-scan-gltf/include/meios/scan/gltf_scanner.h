#ifndef HPP_GUARD_MEIOS_SCAN_GLTF_SCANNER_H
#define HPP_GUARD_MEIOS_SCAN_GLTF_SCANNER_H

#include "meios/io/resolved_asset.h"

#include "meios/diagnostic/log_sink.h"

#include <string>
#include <vector>

namespace meios
{

class scanner_registry;

// glTF/GLB scanner: reports only external images[].uri and buffers[].uri. base64
// data: URIs and the embedded GLB BIN chunk are validated but never reported, so a
// fully-embedded .glb resolves to zero external assets.
class gltf_scanner
{
public:
    std::vector<std::string> scan(const resolved_asset &asset, log_sink &log);
};

void register_gltf_scanner(scanner_registry &registry);

}

#endif
