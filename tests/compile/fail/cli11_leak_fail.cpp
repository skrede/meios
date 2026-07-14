#include "meios/completion.h"

// Guard: no meios-completion header may transitively expose a CLI11 type. This TU
// includes the completion umbrella and then names CLI::App without including any
// CLI11 header, so it must fail to compile. If a library header ever leaked CLI11,
// this reference would resolve, the TU would compile, and its WILL_FAIL test flips.
CLI::App leaked;
