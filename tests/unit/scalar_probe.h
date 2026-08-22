#ifndef HPP_GUARD_MEIOS_UNIT_SCALAR_PROBE_H
#define HPP_GUARD_MEIOS_UNIT_SCALAR_PROBE_H

#include <meios/xacro.h>

#ifdef MEIOS_TEST_HAS_YAML
    #include <meios/yaml/parser.h>
#endif

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <functional>

#ifdef MEIOS_TEST_HAS_YAML

namespace scalar_probe
{

struct recorder
{
    std::vector<std::string> messages;

    void operator()(meios::level lvl, const std::string &message)
    {
        if(lvl == meios::level::error)
            messages.push_back(message);
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        (*this)(lvl, message);
    }

    void operator()(meios::level lvl, meios::diagnostic_code, const meios::source_location &,
                    const std::string &message)
    {
        (*this)(lvl, message);
    }
};

struct outcome
{
    std::optional<meios::value> resolved;
    meios::yaml_failure failure;
    std::vector<std::string> messages;
};

// Every probe goes through a whole document rather than through a private entry point, because
// the plain-versus-quoted distinction the record turns on exists only in the node stream.
inline outcome resolve(const std::string &source)
{
    const std::shared_ptr<const meios::yaml_parser_handle> parser = meios::make_yaml_parser();
    const meios::evaluator_limits ceilings;
    meios::evaluator_counters counters;
    recorder heard;
    meios::log_sink_f sink{ std::ref(heard) };
    const std::string document = "probe: " + source + '\n';
    const meios::yaml_outcome out = (*parser)(document, ceilings, counters, sink, {});
    if(!out.parsed)
        return outcome{ std::nullopt, out.failure, heard.messages };
    return outcome{ out.parsed->at("probe"), out.failure, heard.messages };
}

// The record spells a floating-point result the way the upstream loader's own type is named;
// the value model calls the same kind real.
inline std::string kind_column(meios::value_kind kind)
{
    return kind == meios::value_kind::real ? "float" : std::string(meios::kind_name(kind));
}

}

#endif

#endif
