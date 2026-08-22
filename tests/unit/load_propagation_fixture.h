#ifndef HPP_GUARD_MEIOS_UNIT_LOAD_PROPAGATION_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_LOAD_PROPAGATION_FIXTURE_H

#include <meios/xacro.h>

#include <meios/io/source_handle.h>
#include <meios/io/source_stack.h>
#include <meios/io/directory_source.h>

#include <meios/diagnostic/log_sink.h>

#include <string>
#include <vector>
#include <random>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace
{

std::string read_text(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::filesystem::path fresh_tree()
{
    std::random_device device;
    for(;;)
    {
        const std::filesystem::path candidate = std::filesystem::temp_directory_path() / ("meios-load-" + std::to_string(device()));
        if(!std::filesystem::exists(candidate))
            return candidate;
    }
}

// The expansion a load drives, run directly over the same bytes and the same roots, so a case
// can compare the two error arms as structured values rather than as two message strings.
meios::expected<meios::expansion, meios::expansion_error> expand_directly(const std::filesystem::path &document, const std::vector<std::filesystem::path> &roots, meios::log_sink &log)
{
    meios::source_stack sources;
    for(const std::filesystem::path &root : roots)
        sources.push_back(meios::source_handle(meios::directory_source(root, log)));
    meios::eval_scope scope;
    return meios::expand(read_text(document), scope, sources, document, meios::expansion_limits{}, log);
}

}

#endif
