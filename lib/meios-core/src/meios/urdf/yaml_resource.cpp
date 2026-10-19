#include "yaml_resource.h"
#include "yaml_package_resource.h"

#include "meios/io/text_reader.h"
#include "meios/io/source_stack.h"
#include "meios/io/directory_source.h"
#include "meios/io/beside_containment.h"
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

void remember(sweep &seen, const text_read_failure &failure)
{
    if(!seen.first_failure)
        seen.first_failure = failure;
}

bool present(const std::filesystem::path &path, sweep &seen)
{
    std::error_code error;
    const std::filesystem::file_status status = std::filesystem::status(path, error);
    const bool absent                         = error == std::errc::no_such_file_or_directory || error == std::errc::not_a_directory;
    if(error && !absent)
        remember(seen, {text_read_failure_kind::status, {operation_kind::status, error}});
    else if(absent || !std::filesystem::exists(status))
        seen.absent = true;
    return !error && std::filesystem::exists(status);
}

// A root holding the resource past the read ceiling ends the sweep: the copy it holds is the one
// the spec names, so a later root's copy must not stand in for it, and the crossing outranks any
// native failure an earlier root recovered from.
std::optional<text_resource_result> settle(text_read_result text, sweep &seen)
{
    if(text)
        return text_resource_result{std::move(*text)};
    if(text.error().kind == text_read_failure_kind::too_large)
        return text_resource_result{unexpected<text_resource_failure>(text_resource_failure::exhausted)};
    remember(seen, text.error());
    return std::nullopt;
}

std::optional<text_resource_result> read_contained(const contained_read_result &contained, sweep &seen)
{
    if(!contained)
        remember(seen, {text_read_failure_kind::status, contained.error()});
    else if(!*contained)
        seen.uncontained = true;
    else if(const contained_read &found = **contained; present(found.real, seen))
        return settle(read_text_file_under(found.root, found.real.lexically_relative(found.root), seen.operations, seen.maximum), seen);
    return std::nullopt;
}

std::optional<text_resource_result> from_root(std::string_view spec, const std::filesystem::path &root, sweep &seen)
{
    const std::filesystem::path base = absolute_base(root);
    return read_contained(contain_under(base, base / std::filesystem::path(spec)), seen);
}

text_resource_result refuse_uncontained(std::string_view spec, log_sink &log)
{
    log.log(level::error, diagnostic_code::uncontained_asset, source_location{}, "refused resource \"" + std::string(spec) + "\" outside containment roots");
    return unexpected<text_resource_failure>(text_resource_failure::uncontained);
}

// A determined native cause outranks both verdicts; between the two, absence wins, because a root
// that holds the place for the resource has answered the containment question the other one raised.
text_resource_result conclude_sweep(std::string_view spec, const sweep &seen, log_sink &log)
{
    if(seen.first_failure)
    {
        yaml_package::report_failure(spec, *seen.first_failure, log);
        return unexpected<text_resource_failure>(text_resource_failure::unreachable);
    }
    if(seen.uncontained && !seen.absent)
        return refuse_uncontained(spec, log);
    log.log(level::error, diagnostic_code::unresolved_asset, source_location{}, "could not resolve resource \"" + std::string(spec) + '"');
    return unexpected<text_resource_failure>(text_resource_failure::unreachable);
}

class yaml_fetcher final : public text_resource_loader::fetcher
{
public:
    yaml_fetcher(source_stack &sources, const std::vector<std::filesystem::path> &roots, log_sink &log, const text_reader_operations &operations, yaml_text_delivery_probe &probe,
                 std::size_t maximum, const include_options &includes)
            : m_log(log)
            , m_maximum(maximum)
            , m_includes(includes)
            , m_probe(probe)
            , m_sources(sources)
            , m_operations(operations)
            , m_roots(roots)
    {
    }

    text_resource_result fetch(std::string_view spec, const std::filesystem::path &document) override
    {
        text_resource_result text = yaml_package::matches(spec) ? yaml_package::fetch(spec, m_sources, m_log, m_operations, m_maximum) : from_containment(spec, document);
        if(text)
            m_probe.delivered();
        return text;
    }

private:
    log_sink &m_log;
    std::size_t m_maximum;
    include_options m_includes;
    yaml_text_delivery_probe &m_probe;
    source_stack &m_sources;
    const text_reader_operations &m_operations;
    const std::vector<std::filesystem::path> &m_roots;

    // A link beside the document that leads out of both of its directories names the file the spelling means, so unless the caller follows it,
    // it refuses here rather than letting a configured root's same-named copy stand in.
    std::optional<text_resource_result> from_document(std::string_view spec, const std::filesystem::path &document, sweep &seen)
    {
        const std::filesystem::path base                = absolute_base(document);
        const contained_read_result contained           = contain_beside(base, std::filesystem::path(spec));
        const std::optional<std::filesystem::path> link = contained && !*contained ? beside_link(base, std::filesystem::path(spec)) : std::nullopt;
        if(!link)
            return read_contained(contained, seen);
        if(!m_includes.follow_outside_links)
            return refuse_uncontained(spec, m_log);
        return present(*link, seen) ? settle(read_text_file(*link, m_operations, m_maximum), seen) : std::nullopt;
    }

    text_resource_result from_containment(std::string_view spec, const std::filesystem::path &document)
    {
        sweep seen{false, false, m_maximum, m_operations, std::nullopt};
        std::optional<text_resource_result> settled = document.empty() ? std::nullopt : from_document(spec, document, seen);
        for(std::size_t index = 0; !settled && index < m_roots.size(); ++index)
            settled = from_root(spec, m_roots[index], seen);
        return settled ? std::move(*settled) : conclude_sweep(spec, seen, m_log);
    }
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
                                           yaml_text_delivery_probe &probe, std::size_t maximum, const include_options &includes)
{
    return text_resource_loader{std::make_unique<yaml_fetcher>(sources, roots, log, operations, probe, maximum, includes)};
}

text_resource_loader make_yaml_text_loader(source_stack &sources, const std::vector<std::filesystem::path> &roots, log_sink &log, std::size_t maximum, const include_options &includes)
{
    static null_delivery_probe probe;
    return make_yaml_text_loader(sources, roots, log, default_text_reader_operations(), probe, maximum, includes);
}

}
