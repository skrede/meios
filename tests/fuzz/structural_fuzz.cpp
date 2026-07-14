#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#include <meios/diagnostic/log_sink.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

// The budget and cycle guard guarantee expansion terminates for any input bytes;
// libFuzzer reports a hang or crash if that invariant is ever broken.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    std::string_view source(reinterpret_cast<const char *>(data), size);
    meios::log_sink silent;
    meios::source_stack sources;
    meios::eval_scope scope;
    meios::expansion result =
        meios::expand(source, scope, sources, "fuzz.xacro", meios::expansion_limits{}, silent);
    (void)result;
    return 0;
}
