#include "load_stage.h"

#include "meios/urdf/load.h"

#include "meios/io/source_stack.h"
#include "meios/io/source_handle.h"
#include "meios/io/directory_source.h"

#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/capturing_log_sink.h"

#include <vector>
#include <filesystem>

namespace meios
{

namespace
{

source_stack build_sources(const std::vector<std::filesystem::path> &roots, log_sink &log)
{
    source_stack sources;
    for(const std::filesystem::path &root : roots)
        sources.push_back(source_handle(directory_source(root, log)));
    return sources;
}

}

expected<load_result, load_error> load(const std::filesystem::path &path,
                                       const load_options &opts, log_sink &log)
{
    // Wrap before building sources so a directory_source containment rejection, which
    // it emits to the sink it was constructed with, is counted at the load boundary
    // rather than escaping to the raw sink as a silent error.
    capturing_log_sink wrapper(log);
    source_stack sources = build_sources(opts.package_roots, wrapper);
    return detail::drive_load(path, opts, sources, wrapper);
}

expected<load_result, load_error> load(const std::filesystem::path &path,
                                       const load_options &opts, source_stack &sources,
                                       log_sink &log)
{
    capturing_log_sink wrapper(log);
    return detail::drive_load(path, opts, sources, wrapper);
}

expected<load_result, load_error> load(const std::filesystem::path &path, const load_options &opts)
{
    log_sink silent;
    return load(path, opts, silent);
}

}
