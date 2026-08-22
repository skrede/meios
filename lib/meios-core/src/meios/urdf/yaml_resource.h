#ifndef HPP_GUARD_MEIOS_URDF_YAML_RESOURCE_H
#define HPP_GUARD_MEIOS_URDF_YAML_RESOURCE_H

#include "meios/xacro/evaluator_limits.h"
#include "meios/xacro/text_resource_loader.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"

#include <string>
#include <vector>
#include <cstddef>
#include <filesystem>
#include <string_view>

namespace meios
{
class source_stack;
}

namespace meios::detail
{

class text_reader_operations;

class yaml_text_delivery_probe
{
public:
    yaml_text_delivery_probe()          = default;
    virtual ~yaml_text_delivery_probe() = default;

    virtual void delivered() = 0;

protected:
    yaml_text_delivery_probe(const yaml_text_delivery_probe &)            = default;
    yaml_text_delivery_probe &operator=(const yaml_text_delivery_probe &) = default;
    yaml_text_delivery_probe(yaml_text_delivery_probe &&)                 = default;
    yaml_text_delivery_probe &operator=(yaml_text_delivery_probe &&)      = default;
};

// A resource past the read maximum crossed the evaluator's byte axis rather than failing at the
// device, so it reports on the budget code and names the ceiling the load was given.
inline void report_too_large(std::string_view subject, std::size_t maximum, log_sink &log)
{
    log.log(level::error, diagnostic_code::expansion_budget_exceeded, source_location{},
            "resource \"" + std::string(subject) + "\" is larger than the evaluator's byte ceiling of " + std::to_string(maximum) + " bytes; this load cannot continue");
}

// The returned loader captures all three references; every one of them outlives the evaluation
// scope it is installed on, which is a local of the frame that owns them. It captures no
// document: the file a relative spec belongs to changes as an expansion descends into an
// include, so the document arrives with each request. The maximum bounds one read, which is
// where an auxiliary document is allocated; the parser's own charge still runs afterwards.
text_resource_loader make_yaml_text_loader(source_stack &sources, const std::vector<std::filesystem::path> &roots, log_sink &log, std::size_t maximum = default_byte_ceiling);

text_resource_loader make_yaml_text_loader(source_stack &sources, const std::vector<std::filesystem::path> &roots, log_sink &log, const text_reader_operations &operations,
                                           yaml_text_delivery_probe &probe, std::size_t maximum = default_byte_ceiling);

}

#endif
