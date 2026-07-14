#include <meios/xacro.h>

#include <meios/diagnostic/log_sink.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

// The evaluator must never invoke undefined behavior on arbitrary bytes; a loud
// failure result is acceptable, an ASan/UBSan trap is not.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    std::string_view expression(reinterpret_cast<const char *>(data), size);
    meios::core_evaluator evaluator;
    meios::eval_scope scope;
    meios::log_sink silent;
    meios::value result = evaluator.eval(expression, scope, silent);
    (void)result;
    return 0;
}
