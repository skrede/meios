#include "value_builder.h"
#include "document_reader.h"

#include "meios/xacro/value.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/diagnostic_code.h"

#include <yaml-cpp/parser.h>
#include <yaml-cpp/exceptions.h>

#include <string>
#include <istream>
#include <sstream>
#include <optional>
#include <string_view>

namespace meios::detail
{
namespace
{

yaml_outcome over_byte_ceiling(log_sink &log, const source_location &at)
{
    log.log(level::error, diagnostic_code::expansion_budget_exceeded, at,
            "the evaluator's byte ceiling was reached; this load cannot continue");
    return yaml_outcome{ std::nullopt, yaml_failure::exhausted };
}

yaml_outcome read_events(std::istream &in, value_builder &builder, log_sink &log,
                         const source_location &at)
{
    YAML::Parser reader(in);
    if(!reader.HandleNextDocument(builder))
        return yaml_outcome{ value{}, yaml_failure::none };
    if(reader)
    {
        log.log(level::error, diagnostic_code::expression_error, at,
                "the auxiliary document holds more than one document");
        return yaml_outcome{ std::nullopt, yaml_failure::refused };
    }
    return yaml_outcome{ builder.result(), yaml_failure::none };
}

}

// The seam hands over bytes and never a path, so nothing here opens, resolves or names a file;
// the byte ceiling is charged before the parser is given anything to read.
yaml_outcome yaml_document_reader::parse(std::string_view bytes, const evaluator_limits &limits,
                                         evaluator_counters &counters, log_sink &log,
                                         const source_location &at) const
{
    if(bytes.size() > limits.bytes - counters.bytes)
        return over_byte_ceiling(log, at);
    counters.bytes += bytes.size();
    std::istringstream in{ std::string(bytes) };
    value_builder builder(limits, counters, log, at);
    try
    {
        return read_events(in, builder, log, at);
    }
    catch(const parse_stopped &stop)
    {
        return yaml_outcome{ std::nullopt, stop.why };
    }
    catch(const YAML::Exception &bad)
    {
        log.log(level::error, diagnostic_code::expression_error, at,
                "the auxiliary document does not parse: " + std::string(bad.what()));
        return yaml_outcome{ std::nullopt, yaml_failure::refused };
    }
}

}
