#ifndef HPP_GUARD_MEIOS_URDF_YAML_RESOURCE_H
#define HPP_GUARD_MEIOS_URDF_YAML_RESOURCE_H

#include "meios/xacro/text_resource_loader.h"

#include "meios/diagnostic/log_sink.h"

#include <vector>
#include <filesystem>

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

// The returned loader captures all three arguments by reference; every one of them
// outlives the evaluation scope it is installed on, which is a local of the frame that
// owns them. It captures no document: the file a relative spec belongs to changes as an
// expansion descends into an include, so the document arrives with each request.
text_resource_loader make_yaml_text_loader(source_stack &sources, const std::vector<std::filesystem::path> &roots, log_sink &log);

text_resource_loader make_yaml_text_loader(source_stack &sources, const std::vector<std::filesystem::path> &roots, log_sink &log, const text_reader_operations &operations,
                                           yaml_text_delivery_probe &probe);

}

#endif
