#include "yaml_resource.h"
#include "yaml_package_resource.h"

#include "meios/io/text_reader.h"
#include "meios/io/source_stack.h"
#include "meios/io/directory_source.h"
#include "meios/io/text_reader_operations.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"
#include "meios/diagnostic/diagnostic_code.h"
#include "meios/diagnostic/source_location.h"

#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace meios::detail
{
namespace
{

void report_failure(std::string_view subject, const operation_failure &cause, log_sink &log)
{
    log.log(level::error, diagnostic_code::cannot_open, source_location{}, cause,
            "cannot " + std::string(to_string(cause.operation)) + " resource \"" + std::string(subject) + "\": " + cause.native.message());
}

std::vector<std::filesystem::path> probe_roots(const std::filesystem::path &document, const std::vector<std::filesystem::path> &roots)
{
    std::vector<std::filesystem::path> probes;
    probes.reserve(roots.size() + 1);
    if(!document.empty())
        probes.push_back(absolute_base(document).parent_path());
    for(const std::filesystem::path &root : roots)
        probes.push_back(absolute_base(root));
    return probes;
}

expected<std::filesystem::path, operation_failure> root_request(std::string_view spec, const std::filesystem::path &root, const std::filesystem::path &presentation)
{
    const std::filesystem::path requested{spec};
    if(requested.is_relative())
        return requested;
    std::error_code error;
    const std::filesystem::path base = std::filesystem::weakly_canonical(root, error);
    if(error)
        return unexpected<operation_failure>({operation_kind::canonicalize, error});
    return presentation.lexically_relative(base);
}

expected<std::optional<std::string>, operation_failure> try_root(std::string_view spec, const std::filesystem::path &root, const text_reader_operations &operations, bool &rejected)
{
    contained_path_result contained = try_contained_under(root, root / std::filesystem::path(spec));
    if(!contained)
        return unexpected<operation_failure>(contained.error());
    if(!*contained)
    {
        rejected = true;
        return std::optional<std::string>{};
    }
    std::error_code error;
    const std::filesystem::file_status status = std::filesystem::status(**contained, error);
    if(error == std::errc::no_such_file_or_directory || error == std::errc::not_a_directory)
        return std::optional<std::string>{};
    if(error)
        return unexpected<operation_failure>({operation_kind::status, error});
    if(!std::filesystem::exists(status))
        return std::optional<std::string>{};
    expected<std::filesystem::path, operation_failure> relative = root_request(spec, root, **contained);
    if(!relative)
        return unexpected<operation_failure>(relative.error());
    text_read_result text = read_text_file_under(root, *relative, operations);
    return text ? expected<std::optional<std::string>, operation_failure>{std::optional{std::move(*text)}}
                : expected<std::optional<std::string>, operation_failure>{unexpected<operation_failure>(text.error().cause)};
}

void remember(const operation_failure &failure, std::optional<operation_failure> &first)
{
    if(!first)
        first = failure;
}

std::optional<std::string> from_containment(std::string_view spec, const std::filesystem::path &document, const std::vector<std::filesystem::path> &roots, log_sink &log,
                                            const text_reader_operations &operations)
{
    bool rejected = false;
    std::optional<operation_failure> first_failure;
    for(const std::filesystem::path &root : probe_roots(document, roots))
    {
        expected<std::optional<std::string>, operation_failure> result = try_root(spec, root, operations, rejected);
        if(!result)
            remember(result.error(), first_failure);
        else if(*result)
            return std::move(**result);
    }
    if(first_failure)
        report_failure(spec, *first_failure, log);
    else if(rejected)
        log.log(level::error, diagnostic_code::uncontained_asset, source_location{}, "refused resource \"" + std::string(spec) + "\" outside containment roots");
    else
        log.log(level::error, "could not resolve resource \"" + std::string(spec) + '"');
    return std::nullopt;
}

class yaml_fetcher final : public text_resource_loader::fetcher
{
public:
    yaml_fetcher(source_stack &sources, const std::vector<std::filesystem::path> &roots, log_sink &log, const text_reader_operations &operations, yaml_text_delivery_probe &probe)
            : m_log(log)
            , m_probe(probe)
            , m_sources(sources)
            , m_operations(operations)
            , m_roots(roots)
    {
    }

    std::optional<std::string> fetch(std::string_view spec, const std::filesystem::path &document) override
    {
        std::optional<std::string> text =
                yaml_package::matches(spec) ? yaml_package::fetch(spec, m_sources, m_log, m_operations) : from_containment(spec, document, m_roots, m_log, m_operations);
        if(text)
            m_probe.delivered();
        return text;
    }

private:
    log_sink &m_log;
    yaml_text_delivery_probe &m_probe;
    source_stack &m_sources;
    const text_reader_operations &m_operations;
    const std::vector<std::filesystem::path> &m_roots;
};

class null_delivery_probe final : public yaml_text_delivery_probe
{
public:
    void delivered() override
    {
    }
};

}

text_resource_loader make_yaml_text_loader(source_stack &sources, const std::vector<std::filesystem::path> &roots, log_sink &log, const text_reader_operations &operations,
                                           yaml_text_delivery_probe &probe)
{
    return text_resource_loader{std::make_unique<yaml_fetcher>(sources, roots, log, operations, probe)};
}

text_resource_loader make_yaml_text_loader(source_stack &sources, const std::vector<std::filesystem::path> &roots, log_sink &log)
{
    static null_delivery_probe probe;
    return make_yaml_text_loader(sources, roots, log, default_text_reader_operations(), probe);
}

}
