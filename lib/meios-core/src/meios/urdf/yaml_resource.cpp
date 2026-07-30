#include "yaml_resource.h"

#include "meios/io/text_reader.h"
#include "meios/io/source_stack.h"
#include "meios/io/source_lookup.h"
#include "meios/io/resolved_asset.h"
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
constexpr std::string_view package_prefix = "package://";
constexpr std::string_view find_prefix    = "$(find ";
struct package_ref
{
    std::string package;
    std::string relative;
};
std::optional<package_ref> package_split(std::string_view spec)
{
    spec.remove_prefix(package_prefix.size());
    const std::size_t slash = spec.find('/');
    if(slash == std::string_view::npos)
        return std::nullopt;
    return package_ref{std::string(spec.substr(0, slash)), std::string(spec.substr(slash + 1))};
}
std::optional<package_ref> find_split(std::string_view spec)
{
    spec.remove_prefix(find_prefix.size());
    const std::size_t close = spec.find(')');
    if(close == std::string_view::npos)
        return std::nullopt;
    const std::string_view rest = spec.substr(close + 1);
    if(rest.empty() || (rest.front() != '/' && rest.front() != '\\'))
        return std::nullopt;
    return package_ref{std::string(spec.substr(0, close)), std::string(rest.substr(1))};
}
void report_failure(std::string_view subject, const operation_failure &cause, log_sink &log)
{
    log.log(level::error, diagnostic_code::cannot_open, source_location{}, cause,
            "cannot " + std::string(to_string(cause.operation)) + " resource \"" + std::string(subject) + "\": " + cause.native.message());
}
std::optional<std::string> read_path_text(const std::filesystem::path &path, const text_reader_operations &operations, log_sink &log)
{
    text_read_result text = detail::read_text_file(path, operations);
    if(text)
        return std::move(*text);
    report_failure(path.string(), text.error().cause, log);
    return std::nullopt;
}

std::optional<resolved_asset> locate_package(std::string_view spec, const package_ref &ref, source_stack &sources, log_sink &log)
{
    source_lookup_result hit = sources.try_locate(ref.package, ref.relative, log);
    if(!hit)
    {
        report_failure(spec, hit.error(), log);
        return std::nullopt;
    }
    if(!*hit)
    {
        log.log(level::error, "could not resolve resource \"" + std::string(spec) + '"');
        return std::nullopt;
    }
    return std::move(**hit);
}

std::optional<std::string> from_package(std::string_view spec, source_stack &sources, log_sink &log, const text_reader_operations &operations)
{
    const std::optional<package_ref> ref = spec.starts_with(package_prefix) ? package_split(spec) : find_split(spec);
    if(!ref)
    {
        log.log(level::error, "malformed package resource spec \"" + std::string(spec) + '"');
        return std::nullopt;
    }
    const std::optional<resolved_asset> hit = locate_package(spec, *ref, sources, log);
    return hit ? read_path_text(hit->path(), operations, log) : std::nullopt;
}
std::vector<std::filesystem::path> probe_roots(const std::filesystem::path &document, const std::vector<std::filesystem::path> &roots)
{
    std::vector<std::filesystem::path> probes;
    probes.reserve(roots.size() + 1);
    if(!document.empty())
        probes.push_back(document.parent_path());
    probes.insert(probes.end(), roots.begin(), roots.end());
    return probes;
}
std::optional<std::filesystem::path> contained_spec(std::string_view spec, const std::filesystem::path &document, const std::vector<std::filesystem::path> &roots)
{
    log_sink quiet;
    for(const std::filesystem::path &root : probe_roots(document, roots))
    {
        const std::optional<std::filesystem::path> candidate = contained_candidate(root, "", spec, quiet);
        std::error_code error;
        if(candidate && std::filesystem::exists(*candidate, error))
            return candidate;
    }
    return std::nullopt;
}
std::optional<std::string> from_containment(std::string_view spec, const std::filesystem::path &document, const std::vector<std::filesystem::path> &roots, log_sink &log,
                                            const text_reader_operations &operations)
{
    const std::optional<std::filesystem::path> path = contained_spec(spec, document, roots);
    if(!path)
    {
        log.log(level::error, "refused resource \"" + std::string(spec) + "\" that resolves outside every containment root");
        return std::nullopt;
    }
    return read_path_text(*path, operations, log);
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
        std::optional<std::string> text = spec.starts_with(package_prefix) || spec.starts_with(find_prefix) ? from_package(spec, m_sources, m_log, m_operations)
                                                                                                            : from_containment(spec, document, m_roots, m_log, m_operations);
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
