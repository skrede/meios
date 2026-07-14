#include "meios/scan/stl_scanner.h"

#include "meios/bundle/scanner_handle.h"
#include "meios/bundle/scanner_registry.h"

namespace meios
{

void register_stl_scanner(scanner_registry &registry)
{
    registry.register_scanner("stl", scanner_handle{ stl_scanner{} });
}

}
