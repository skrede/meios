#ifndef HPP_GUARD_MEIOS_URDF_YAML_SWEEP_H
#define HPP_GUARD_MEIOS_URDF_YAML_SWEEP_H

#include "meios/io/text_reader.h"
#include "meios/io/beside_containment.h"
#include "meios/io/text_reader_operations.h"

#include "meios/xacro/text_resource_loader.h"

#include "meios/diagnostic/log_sink.h"

#include <cstddef>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail::yaml_sweep
{

// What the sweep over the probe roots determined, kept per verdict rather than as one flag: a
// resource one root holds the place for and another excludes is missing, not escaping.
struct sweep
{
    bool absent;
    bool uncontained;
    std::size_t maximum;
    const text_reader_operations &operations;
    std::optional<text_read_failure> first_failure;
};

void remember(sweep &seen, const text_read_failure &failure);

bool present(const std::filesystem::path &path, sweep &seen);

std::optional<text_resource_result> settle(text_read_result text, sweep &seen);

std::optional<text_resource_result> read_contained(const contained_read_result &contained, sweep &seen);

std::optional<text_resource_result> from_root(std::string_view spec, const std::filesystem::path &root, sweep &seen);

bool passes_link(const std::filesystem::path &document, std::string_view spec, sweep &seen);

text_resource_result refuse_uncontained(std::string_view spec, log_sink &log);

text_resource_result conclude_sweep(std::string_view spec, const sweep &seen, log_sink &log);

}

#endif
