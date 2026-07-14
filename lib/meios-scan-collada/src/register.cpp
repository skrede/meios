#include "meios/scan/collada_scanner.h"

#include "meios/bundle/scanner_handle.h"
#include "meios/bundle/scanner_registry.h"

namespace meios
{

void register_collada_scanner(scanner_registry &registry)
{
    registry.register_scanner("dae", scanner_handle{ collada_scanner{} });
}

}
