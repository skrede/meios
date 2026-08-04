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

// What the sweep over the probe roots determined, kept per verdict rather than as one flag: a
// resource one root holds the place for and another excludes is missing, not escaping.
struct sweep
{
    bool absent;
    bool uncontained;
};

expected<std::optional<std::string>, operation_failure> nothing(bool &verdict)
{
    verdict = true;
    return std::optional<std::string>{};
}

expected<std::optional<std::string>, operation_failure> try_root(std::string_view spec, const std::filesystem::path &root, const text_reader_operations &operations, sweep &seen)
{
    contained_path_result contained = try_contained_under(root, root / std::filesystem::path(spec));
    if(!contained)
        return unexpected<operation_failure>(contained.error());
    if(!*contained)
        return nothing(seen.uncontained);
    std::error_code error;
    const std::filesystem::file_status status = std::filesystem::status(**contained, error);
    if(error == std::errc::no_such_file_or_directory || error == std::errc::not_a_directory)
        return nothing(seen.absent);
    if(error)
        return unexpected<operation_failure>({operation_kind::status, error});
    if(!std::filesystem::exists(status))
        return nothing(seen.absent);
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

// A determined native cause outranks both verdicts; between the two, absence wins, because a root
// that holds the place for the resource has answered the containment question the other one raised.
void report_exhausted(std::string_view spec, const sweep &seen, const std::optional<operation_failure> &first_failure, log_sink &log)
{
    if(first_failure)
        report_failure(spec, *first_failure, log);
    else if(seen.uncontained && !seen.absent)
        log.log(level::error, diagnostic_code::uncontained_asset, source_location{}, "refused resource \"" + std::string(spec) + "\" outside containment roots");
    else
        log.log(level::error, diagnostic_code::unresolved_asset, source_location{}, "could not resolve resource \"" + std::string(spec) + '"');
}

std::optional<std::string> from_containment(std::string_view spec, const std::filesystem::path &document, const std::vector<std::filesystem::path> &roots, log_sink &log,
                                            const text_reader_operations &operations)
{
    sweep seen{false, false};
    std::optional<operation_failure> first_failure;
    for(const std::filesystem::path &root : probe_roots(document, roots))
    {
        expected<std::optional<std::string>, operation_failure> result = try_root(spec, root, operations, seen);
        if(!result)
            remember(result.error(), first_failure);
        else if(*result)
            return std::move(**result);
    }
    report_exhausted(spec, seen, first_failure, log);
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
