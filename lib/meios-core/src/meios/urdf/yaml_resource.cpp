#include "yaml_resource.h"
#include "yaml_sweep.h"
#include "yaml_package_resource.h"

#include "meios/io/text_reader.h"
#include "meios/io/source_stack.h"
#include "meios/io/directory_source.h"
#include "meios/io/beside_containment.h"
#include "meios/io/text_reader_operations.h"

#include "meios/diagnostic/log_sink.h"

#include <memory>
#include <vector>
#include <cstddef>
#include <utility>
#include <optional>
#include <filesystem>
#include <string_view>

namespace meios::detail
{
namespace
{

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

    // A link beside the document that leads out of every directory admitting it names the file the spelling means, so it refuses here unless the caller follows it; a followed link's
    // answer, a dangling one's included, is final, as is a spelling through an element that is not a directory, so a configured root's same-named copy never stands in for either.
    std::optional<text_resource_result> from_document(std::string_view spec, const std::filesystem::path &document, yaml_sweep::sweep &seen)
    {
        const std::filesystem::path base                = absolute_base(document);
        const contained_read_result contained           = contain_beside(base, std::filesystem::path(spec), m_roots);
        const std::optional<std::filesystem::path> link = contained && !*contained ? beside_link(base, std::filesystem::path(spec)) : std::nullopt;
        if(link && !m_includes.follow_outside_links)
            return yaml_sweep::refuse_uncontained(spec, m_log);
        std::optional<text_resource_result> read = link ? std::nullopt : yaml_sweep::read_contained(contained, seen);
        if(link && yaml_sweep::present(*link, seen))
            read = yaml_sweep::settle(read_text_file(*link, m_operations, m_maximum), seen);
        const bool refused = !contained && through_non_directory(contained.error());
        const bool walked  = m_includes.follow_outside_links && !read && !link && yaml_sweep::passes_link(base, spec, seen);
        if(!refused && (read || !m_includes.follow_outside_links || !(link || walked)))
            return read;
        return yaml_sweep::conclude_sweep(spec, seen, m_log);
    }

    text_resource_result from_containment(std::string_view spec, const std::filesystem::path &document)
    {
        yaml_sweep::sweep seen{false, false, m_maximum, m_operations, std::nullopt};
        std::optional<text_resource_result> settled = document.empty() ? std::nullopt : from_document(spec, document, seen);
        for(std::size_t index = 0; !settled && index < m_roots.size(); ++index)
            settled = yaml_sweep::from_root(spec, m_roots[index], seen);
        return settled ? std::move(*settled) : yaml_sweep::conclude_sweep(spec, seen, m_log);
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
