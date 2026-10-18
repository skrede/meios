#ifndef HPP_GUARD_MEIOS_TEST_NATIVE_MEMBER_FIXTURE_H
#define HPP_GUARD_MEIOS_TEST_NATIVE_MEMBER_FIXTURE_H

#include "oracle_records.h"
#include "fake_yaml_parser.h"

#include <meios/xacro.h>

#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <utility>
#include <filesystem>
#include <functional>
#include <string_view>

namespace native_member_test
{

inline constexpr std::string_view limits_document = "arm:\n  shoulder:\n    lower: -1.5\n    upper: 1.5\nbounds:\n  - 1\n  - 2\nlabel: text\n";

// Built from the measurement rather than written out, so the document under test carries exactly
// the names upstream was observed to answer itself, plus one name reached by the prefix rule.
inline std::string measured_document()
{
    std::string out = "__class__: -1\n";
    std::size_t at  = 0;
    for(const oracle::row &one : oracle::load_rows("collisions.cases"))
        out += one.fields.front() + ": " + std::to_string(at++) + '\n';
    return out;
}

class two_documents final : public meios::text_resource_loader::fetcher
{
public:
    meios::text_resource_result fetch(std::string_view spec, const std::filesystem::path &) override
    {
        if(spec == "measured.yaml")
            return measured_document();
        return std::string(limits_document);
    }
};

struct recorder
{
    std::vector<std::string> messages;
    std::vector<meios::diagnostic_code> codes;

    void operator()(meios::level lvl, const std::string &message)
    {
        if(lvl == meios::level::error)
            messages.push_back(message);
    }

    void operator()(meios::level lvl, const meios::source_location &, const std::string &message)
    {
        (*this)(lvl, message);
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &, const std::string &message)
    {
        if(lvl == meios::level::error)
            codes.push_back(code);
        (*this)(lvl, message);
    }
};

struct outcome
{
    bool failed;
    meios::eval_failure_kind kind;
    std::string rendered;
    std::vector<std::string> messages;
    std::vector<meios::diagnostic_code> codes;
};

inline meios::value authored_mapping()
{
    std::vector<meios::value::entry> entries;
    entries.emplace_back("base", meios::value{std::int64_t{1}});
    entries.emplace_back("other", meios::value{std::int64_t{2}});
    return meios::value::make_mapping(std::move(entries));
}

inline meios::eval_scope probe_scope(meios::log_sink &log)
{
    meios::eval_scope scope;
    scope.install_text_loader(meios::text_resource_loader{std::make_unique<two_documents>()});
    scope.install_yaml_parser(fake::yaml_parser_handle());
    scope.set("limits_file", meios::value{std::string("limits.yaml")});
    scope.set("measured_file", meios::value{std::string("measured.yaml")});
    scope.set("authored", authored_mapping());
    meios::core_evaluator evaluator;
    scope.set("config", evaluator.eval("xacro.load_yaml(limits_file)", scope, log));
    scope.set("kit", evaluator.eval("xacro.load_yaml(measured_file)", scope, log));
    return scope;
}

inline outcome evaluate(const std::string &expression)
{
    recorder heard;
    meios::log_sink_f sink{std::ref(heard)};
    const meios::eval_scope scope = probe_scope(sink);
    meios::core_evaluator evaluator;
    const meios::value result = evaluator.eval(expression, scope, sink);
    return outcome{evaluator.failed(), evaluator.failure_kind(), evaluator.failed() ? std::string() : meios::render_scalar(result).value_or("<collection>"), heard.messages,
                   heard.codes};
}

}

#endif
