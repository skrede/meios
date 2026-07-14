#include "meios/scan/gltf_scanner.h"

#include "meios/bundle/scanner_handle.h"
#include "meios/bundle/scanner_registry.h"

namespace meios
{

void register_gltf_scanner(scanner_registry &registry)
{
    registry.register_scanner("gltf", scanner_handle{ gltf_scanner{} });
    registry.register_scanner("glb", scanner_handle{ gltf_scanner{} });
}

}
