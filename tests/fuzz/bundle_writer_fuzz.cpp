#include <meios/bundle.h>
#include <meios/urdf.h>

#include <cstddef>
#include <cstdint>

// The emitter harness lands with the writer body; this scaffold keeps the fuzz
// target live so it activates the moment the emission path exists to drive.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    (void)data;
    (void)size;
    return 0;
}
