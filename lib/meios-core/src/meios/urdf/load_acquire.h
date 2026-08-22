#ifndef HPP_GUARD_MEIOS_URDF_LOAD_ACQUIRE_H
#define HPP_GUARD_MEIOS_URDF_LOAD_ACQUIRE_H

#include "meios/urdf/load.h"
#include "meios/urdf/load_result.h"

#include "meios/diagnostic/load_error.h"
#include "meios/diagnostic/capturing_log_sink.h"

#include "meios/expected.h"

#include <string>
#include <filesystem>

namespace meios
{

class source_stack;

namespace detail
{

class text_reader_operations;

enum class load_stage_point
{
    sniff,
    drive,
    assemble,
};

class load_stage_probe
{
public:
    load_stage_probe()          = default;
    virtual ~load_stage_probe() = default;

    virtual void entered(load_stage_point point) = 0;

protected:
    load_stage_probe(const load_stage_probe &)            = default;
    load_stage_probe &operator=(const load_stage_probe &) = default;
    load_stage_probe(load_stage_probe &&)                 = default;
    load_stage_probe &operator=(load_stage_probe &&)      = default;
};

expected<std::string, load_error> acquire_load_text(const std::filesystem::path &path, capture_window &window);

expected<std::string, load_error> acquire_load_text(const std::filesystem::path &path, capture_window &window, const text_reader_operations &operations);

expected<load_result, load_error> drive_load(const std::filesystem::path &path, const load_options &opts, source_stack &sources, capture_window &window,
                                             const text_reader_operations &operations, load_stage_probe &probe);

}

}

#endif
