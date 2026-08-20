#ifndef HPP_GUARD_MEIOS_UNIT_STRING_PROBE_H
#define HPP_GUARD_MEIOS_UNIT_STRING_PROBE_H

#include "fake_yaml_parser.h"

#include "meios/xacro/eval_session.h"

#include <meios/xacro.h>

#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <optional>
#include <filesystem>
#include <functional>
#include <string_view>

namespace strings
{

constexpr std::string_view parts_document =
    "path: meshes/base.dae\nxyz: 0.1 0.2 0.3\nbounds:\n  - 1\n  - 2\n";

class parts_loader final : public meios::text_resource_loader::fetcher
{
public:
    std::optional<std::string> fetch(std::string_view, const std::filesystem::path &) override
    {
        return std::string(parts_document);
    }
};

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
    bool failed;
    meios::eval_failure_kind kind;
    std::string rendered;
    std::vector<std::string> messages;
};

inline meios::value authored_mapping()
{
    std::vector<meios::value::entry> entries;
    entries.emplace_back("base", meios::value{ std::int64_t{ 1 } });
    return meios::value::make_mapping(std::move(entries));
}

inline meios::eval_scope probe_scope(meios::log_sink &log)
{
    meios::eval_scope scope;
    scope.install_text_loader(meios::text_resource_loader{ std::make_unique<parts_loader>() });
    scope.install_yaml_parser(fake::yaml_parser_handle());
    scope.set("parts_file", meios::value{ std::string("parts.yaml") });
    scope.set("prefix", meios::value{ std::string("arm") });
    scope.set("blank", meios::value{ std::string() });
    scope.set("mass", *meios::value::make_real(3.7));
    scope.set("table", authored_mapping());
    meios::core_evaluator evaluator;
    scope.set("config", evaluator.eval("xacro.load_yaml(parts_file)", scope, log));
    scope.set("loaded", evaluator.eval("config['bounds']", scope, log));
    scope.set("fields", evaluator.eval("config['xyz'].split(' ')", scope, log));
    return scope;
}

inline meios::value evaluated(const std::string &expression, meios::log_sink &log)
{
    meios::core_evaluator evaluator;
    return evaluator.eval(expression, probe_scope(log), log);
}

inline meios::value evaluated(const std::string &expression)
{
    meios::log_sink silent;
    return evaluated(expression, silent);
}

inline outcome evaluate(const std::string &expression)
{
    recorder heard;
    meios::log_sink_f sink{ std::ref(heard) };
    const meios::eval_scope scope = probe_scope(sink);
    meios::core_evaluator evaluator;
    const meios::value result = evaluator.eval(expression, scope, sink);
    return outcome{ evaluator.failed(), evaluator.failure_kind(),
                    evaluator.failed() ? std::string()
                                       : meios::render_scalar(result).value_or("<collection>"),
                    heard.messages };
}

// The fields joined by a character no probe contains, so one comparison pins the count, the
// order and every empty field at once.
inline std::string joined(const meios::value &one)
{
    std::string out;
    for(std::size_t at = 0; at < one.size(); ++at)
        out += (at == 0 ? "" : "|") + one.at(at)->text().value_or("<not text>");
    return out;
}

// A chain of additions charged against one session, which is what bounds how much text one
// expression may produce until a production ceiling of its own exists.
inline bool chain_survives(std::size_t links, std::size_t token_ceiling, std::size_t step_ceiling)
{
    std::string expression = "'xxxxxxxx'";
    for(std::size_t at = 0; at < links; ++at)
        expression += " + 'xxxxxxxx'";
    const meios::evaluator_limits ceilings{ 0, 0, 0, token_ceiling, step_ceiling };
    meios::detail::eval_session session(ceilings);
    meios::eval_scope scope;
    meios::log_sink silent;
    meios::core_evaluator evaluator;
    evaluator.eval(expression, scope, silent, {}, session);
    return !evaluator.failed();
}

}

#endif
