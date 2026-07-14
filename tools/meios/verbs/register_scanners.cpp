#include "register_scanners.h"

#include "meios/bundle/scanner_registry.h"

namespace meios
{

#ifdef MEIOS_CLI_HAS_SCAN_OBJ
void register_obj_scanner(scanner_registry &);
#endif
#ifdef MEIOS_CLI_HAS_SCAN_COLLADA
void register_collada_scanner(scanner_registry &);
#endif
#ifdef MEIOS_CLI_HAS_SCAN_STL
void register_stl_scanner(scanner_registry &);
#endif
#ifdef MEIOS_CLI_HAS_SCAN_GLTF
void register_gltf_scanner(scanner_registry &);
#endif

}

namespace meios::cli
{

void register_scanners(scanner_registry &registry)
{
    (void)registry;
#ifdef MEIOS_CLI_HAS_SCAN_OBJ
    register_obj_scanner(registry);
#endif
#ifdef MEIOS_CLI_HAS_SCAN_COLLADA
    register_collada_scanner(registry);
#endif
#ifdef MEIOS_CLI_HAS_SCAN_STL
    register_stl_scanner(registry);
#endif
#ifdef MEIOS_CLI_HAS_SCAN_GLTF
    register_gltf_scanner(registry);
#endif
}

}
