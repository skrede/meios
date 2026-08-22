#ifndef HPP_GUARD_MEIOS_TEST_NATIVE_YAML_SEAM_FIXTURE_H
#define HPP_GUARD_MEIOS_TEST_NATIVE_YAML_SEAM_FIXTURE_H

#include "meios/urdf/yaml_resource.h"

#include "meios/xacro/eval_session.h"
#include "meios/xacro/substitution_detail.h"

#include "meios/io/text_reader_operations.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#ifdef MEIOS_TEST_HAS_YAML
    #include <meios/yaml/parser.h>
#endif

#include <span>
#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <optional>
#include <algorithm>
#include <stdexcept>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace yaml_seam_test
{

constexpr std::size_t generated_size = std::size_t{ 1 } << 20;
constexpr std::size_t read_ceiling   = std::size_t{ 4 } << 10;

// A file with no bytes behind it: it manufactures a fixed count on demand, so what the reader
// accepts can be measured without a second copy of it existing anywhere.
class generated_file final : public meios::detail::text_file
{
public:
    generated_file(std::size_t size, std::size_t &delivered) : m_remaining(size), m_delivered(delivered) {}

    std::size_t read(std::span<char> buffer) noexcept override
    {
        const std::size_t count = std::min(buffer.size(), m_remaining);
        std::fill_n(buffer.data(), count, 'a');
        m_remaining -= count;
        m_delivered += count;
        return count;
    }

    bool failed() const noexcept override { return false; }
    std::error_code native_error() const noexcept override { return {}; }
    bool close() noexcept override { return true; }

private:
    std::size_t m_remaining;
    std::size_t &m_delivered;
};

class generating_operations final : public meios::detail::text_reader_operations
{
public:
    generating_operations(std::size_t size, std::size_t &delivered) : m_size(size), m_delivered(delivered) {}

    meios::detail::text_status_result status(const std::filesystem::path &) const noexcept override
    {
        return std::filesystem::file_status(std::filesystem::file_type::regular);
    }

    meios::detail::text_file_result open(const std::filesystem::path &) const noexcept override
    {
        return generate();
    }

    meios::detail::text_open_result open_under(const std::filesystem::path &, const std::filesystem::path &) const noexcept override
    {
        return generate();
    }

private:
    std::size_t m_size;
    std::size_t &m_delivered;

    std::unique_ptr<meios::detail::text_file> generate() const
    {
        return std::make_unique<generated_file>(m_size, m_delivered);
    }
};

class null_probe final : public meios::detail::yaml_text_delivery_probe
{
public:
    void delivered() override {}
};

struct recording_sink final : public meios::log_sink
{
    using meios::log_sink::log;

    std::vector<std::string> messages;

    void log(meios::level lvl, meios::diagnostic_code, const meios::source_location &, const std::string &message) override
    {
        if(lvl == meios::level::error)
            messages.push_back(message);
    }
};

#ifdef MEIOS_TEST_HAS_YAML

struct sink_refusal
{
};

class throwing_sink final : public meios::log_sink
{
public:
    using meios::log_sink::log;

    explicit throwing_sink(bool standard) : m_standard(standard) {}

    void log(meios::level, meios::diagnostic_code, const meios::source_location &, const std::string &) override
    {
        if(m_standard)
            throw std::runtime_error("the sink refused the record");
        throw sink_refusal{};
    }

private:
    bool m_standard;
};

class fixed_loader final : public meios::text_resource_loader::fetcher
{
public:
    explicit fixed_loader(std::string text) : m_text(std::move(text)) {}

    std::optional<std::string> fetch(std::string_view, const std::filesystem::path &) override { return m_text; }

private:
    std::string m_text;
};

// The whole path a description takes, so a classification is judged by what the caller's chosen
// policy did with it. Nothing back means the load was terminal; text means the span survived.
inline std::optional<std::string> substitute(std::string_view document, meios::eval_policy policy)
{
    meios::eval_scope scope;
    scope.install_text_loader(meios::text_resource_loader{ std::make_unique<fixed_loader>(std::string(document)) });
    scope.install_yaml_parser(meios::make_yaml_parser());
    scope.set("params_file", meios::value{ std::string("params.yaml") });
    const meios::evaluator_limits ceilings;
    meios::detail::eval_session session(ceilings);
    meios::source_stack sources;
    meios::log_sink sink;
    const meios::expected<meios::substitution, meios::expansion_error> out =
        meios::detail::substitute_refined("v ${xacro.load_yaml(params_file)['a']}", scope, sources, "inline.xacro", policy, {}, session, sink, {});
    return out.has_value() ? std::optional<std::string>(out.value().text) : std::nullopt;
}

#endif

}

#endif
