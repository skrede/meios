#ifndef HPP_GUARD_MEIOS_UNIT_DOCUMENT_PROBE_H
#define HPP_GUARD_MEIOS_UNIT_DOCUMENT_PROBE_H

#include <meios/xacro.h>

#ifdef MEIOS_TEST_HAS_YAML
    #include <meios/yaml/parser.h>
#endif

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <string_view>

namespace document_probe
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

#ifdef MEIOS_TEST_HAS_YAML

struct outcome
{
    std::optional<meios::value> parsed;
    meios::yaml_failure failure;
    std::vector<std::string> messages;
    meios::evaluator_counters counters;
};

inline outcome read(std::string_view document, const meios::evaluator_limits &ceilings)
{
    const std::shared_ptr<const meios::yaml_parser_handle> parser = meios::make_yaml_parser();
    meios::evaluator_counters counters;
    recorder heard;
    meios::log_sink_f sink{ std::ref(heard) };
    const meios::yaml_outcome out = (*parser)(document, ceilings, counters, sink, {});
    return outcome{ out.parsed, out.failure, heard.messages, counters };
}

inline outcome read(std::string_view document)
{
    const meios::evaluator_limits defaults;
    return read(document, defaults);
}

// The entries in source order, spelled the way the key type and the value type spell
// themselves, so a case pins the key order and the resolved values in one comparison.
inline std::string entries_of(const meios::value &one)
{
    std::string out;
    for(std::size_t at = 0; at < one.size(); ++at)
    {
        out += at == 0 ? "" : ",";
        out += meios::render_scalar(*one.key_at(at)) + '=';
        out += meios::render_scalar(*one.at(at)).value_or("<collection>");
    }
    return out;
}

#endif

}

#endif
