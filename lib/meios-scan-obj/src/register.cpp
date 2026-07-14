#include "meios/scan/obj_scanner.h"

#include "meios/bundle/scanner_handle.h"
#include "meios/bundle/scanner_registry.h"

namespace meios
{

void register_obj_scanner(scanner_registry &registry)
{
    registry.register_scanner("obj", scanner_handle{ obj_scanner{} });
    registry.register_scanner("mtl", scanner_handle{ obj_scanner{} });
}

}
