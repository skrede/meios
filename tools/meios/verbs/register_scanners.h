#ifndef HPP_GUARD_MEIOS_CLI_REGISTER_SCANNERS_H
#define HPP_GUARD_MEIOS_CLI_REGISTER_SCANNERS_H

namespace meios
{
class scanner_registry;
}

namespace meios::cli
{

// Populates the registry with every scanner target the CLI was linked against.
// Each per-scanner block is compiled in only when its target is present, so the
// CLI carries no dependency on any concrete scanner it was not built with.
void register_scanners(scanner_registry &registry);

}

#endif
