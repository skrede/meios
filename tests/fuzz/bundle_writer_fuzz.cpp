#include <meios/bundle.h>
#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>
#include <meios/xacro.h>

#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string_view>

// Drive arbitrary bytes through the parse->emit path: any hostile document that
// reaches the writer must escape or hard-fail through the Char gate, never crash
// or leak. libFuzzer under asan reports either.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size)
{
    const std::string_view source(reinterpret_cast<const char *>(data), size);
    meios::log_sink silent;
    meios::source_stack sources;
    meios::core_evaluator eval;
    meios::parse_context ctx{ sources,       eval, silent, meios::missing_asset::warn,
                              meios::topology_policy::fail, meios::material_policy::warn,
                              meios::strictness::fail,      "fuzz.urdf" };
    std::ostringstream out;
    meios::urdf_writer writer(out, silent);
    meios::basic_parser<meios::urdf_reader> parser(ctx);
    parser.parse(source, writer);
    (void)writer.status();
    (void)out.str();
    return 0;
}
