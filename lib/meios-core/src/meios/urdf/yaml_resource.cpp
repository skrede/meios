#include "yaml_resource.h"

#include "meios/io/source_stack.h"
#include "meios/io/resolved_asset.h"
#include "meios/io/directory_source.h"

#include "meios/diagnostic/level.h"
#include "meios/diagnostic/log_sink.h"

#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <optional>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace meios::detail
{

namespace
{

constexpr std::string_view package_prefix = "package://";
constexpr std::string_view find_prefix = "$(find ";

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
    return package_ref{ std::string(spec.substr(0, slash)), std::string(spec.substr(slash + 1)) };
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
    return package_ref{ std::string(spec.substr(0, close)), std::string(rest.substr(1)) };
}

std::optional<std::string> read_path_text(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    if(!in)
        return std::nullopt;
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::optional<std::string> from_package(std::string_view spec, source_stack &sources, log_sink &log)
{
    const std::optional<package_ref> ref =
        spec.starts_with(package_prefix) ? package_split(spec) : find_split(spec);
    if(!ref)
    {
        log.log(level::error, "malformed package resource spec \"" + std::string(spec) + '"');
        return std::nullopt;
    }
    const std::optional<resolved_asset> hit = sources.locate(ref->package, ref->relative, log);
    if(!hit)
    {
        log.log(level::error, "could not resolve resource \"" + std::string(spec) + '"');
        return std::nullopt;
    }
    return read_path_text(hit->path());
}

std::vector<std::filesystem::path> probe_roots(const std::filesystem::path &document,
                                               const std::vector<std::filesystem::path> &roots)
{
    std::vector<std::filesystem::path> probes;
    probes.reserve(roots.size() + 1);
    if(!document.empty())
        probes.push_back(document.parent_path());
    probes.insert(probes.end(), roots.begin(), roots.end());
    return probes;
}

// Containment is judged on the resolved candidate, never on the authored spelling: the two
// substitution entry points hand this resolver different shapes for the same authored form
// — one a pre-resolved absolute path, the other a raw package token — so the rule has to be
// "lands inside a root" rather than "does not look absolute". The probe runs against a
// silent sink because contained_candidate logs its own rejection and several roots would
// otherwise emit one refusal each; the caller emits the single real one.
std::optional<std::filesystem::path> contained_spec(std::string_view spec,
                                                    const std::filesystem::path &document,
                                                    const std::vector<std::filesystem::path> &roots)
{
    log_sink quiet;
    for(const std::filesystem::path &root : probe_roots(document, roots))
    {
        const std::optional<std::filesystem::path> candidate =
            contained_candidate(root, "", spec, quiet);
        std::error_code ec;
        if(candidate && std::filesystem::exists(*candidate, ec))
            return candidate;
    }
    return std::nullopt;
}

std::optional<std::string> from_containment(std::string_view spec,
                                            const std::filesystem::path &document,
                                            const std::vector<std::filesystem::path> &roots,
                                            log_sink &log)
{
    const std::optional<std::filesystem::path> located = contained_spec(spec, document, roots);
    if(!located)
    {
        log.log(level::error, "refused resource \"" + std::string(spec)
                                  + "\" that resolves outside every containment root");
        return std::nullopt;
    }
    return read_path_text(*located);
}

struct yaml_fetcher final : text_resource_loader::fetcher
{
    yaml_fetcher(source_stack &sources, const std::vector<std::filesystem::path> &roots,
                 log_sink &log)
        : m_log(log), m_sources(sources), m_roots(roots)
    {
    }

    std::optional<std::string> fetch(std::string_view spec,
                                     const std::filesystem::path &document) override
    {
        if(spec.starts_with(package_prefix) || spec.starts_with(find_prefix))
            return from_package(spec, m_sources, m_log);
        return from_containment(spec, document, m_roots, m_log);
    }

    log_sink &m_log;
    source_stack &m_sources;
    const std::vector<std::filesystem::path> &m_roots;
};

}

text_resource_loader make_yaml_text_loader(source_stack &sources,
                                          const std::vector<std::filesystem::path> &roots,
                                          log_sink &log)
{
    return text_resource_loader{ std::make_unique<yaml_fetcher>(sources, roots, log) };
}

}
