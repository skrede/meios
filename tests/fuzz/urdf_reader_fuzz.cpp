#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>
#include <meios/xacro.h>

#include <cstddef>
#include <cstdint>
#include <string_view>

// The reader must return a typed result for any input bytes; libFuzzer reports a
// crash or hang if a hostile document ever drives it into UB or non-termination.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    const std::string_view source(reinterpret_cast<const char *>(data), size);
    meios::log_sink silent;
    meios::source_stack sources;
    meios::core_evaluator eval;
    meios::parse_context ctx{ sources,       eval, silent, meios::missing_asset::warn,
                              meios::topology_policy::fail, meios::material_policy::warn,
                              meios::strictness::strict,    "fuzz.urdf" };
    meios::pod_recorder<meios::tree<double>> rec(silent, meios::topology_policy::fail);
    meios::basic_parser<meios::urdf_reader> parser(ctx);
    parser.parse(source, rec);
    (void)rec.result();
    return 0;
}
