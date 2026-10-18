#ifndef HPP_GUARD_MEIOS_TESTS_DIFFERENTIAL_SEED_H
#define HPP_GUARD_MEIOS_TESTS_DIFFERENTIAL_SEED_H

#include "differential_seed_scope.h"

#include <meios/xacro.h>

#include <meios/io/source_stack.h>

#ifdef MEIOS_TEST_HAS_YAML
    #include <meios/yaml/parser.h>
#endif

#include <array>
#include <memory>
#include <string>
#include <filesystem>
#include <string_view>

namespace differential
{

struct outcome
{
    bool failed;
    std::string rendered;
};

// The seeded expression-case scope -- every expressions.cases id is evaluated under it.
inline outcome evaluate_seeded(std::string_view expression)
{
    meios::log_sink silent;
    const meios::eval_scope scope = seeded_scope(silent);
    meios::core_evaluator evaluator;
    const meios::value result = evaluator.eval(expression, scope, silent);
    return { evaluator.failed(), evaluator.failed() ? std::string() : rendered_as(result) };
}

// No seeding: a divergence-manifest probe is a self-contained expression.
inline outcome evaluate_bare(std::string_view expression)
{
    meios::eval_scope scope;
    meios::log_sink silent;
    meios::core_evaluator evaluator;
    const meios::value result = evaluator.eval(expression, scope, silent);
    return { evaluator.failed(), evaluator.failed() ? std::string() : rendered_as(result) };
}

// A span-area difference is about which text the scanner hands the evaluator, so its probe is
// driven through the whole substitution rather than through the grammar alone; the body arrives
// wrapped exactly as differential.py wraps it on the other side.
inline outcome evaluate_span(std::string_view body)
{
    meios::eval_scope scope;
    meios::source_stack sources;
    meios::log_sink silent;
    const meios::expected<meios::substitution, meios::expansion_error> out = meios::substitute(
        "${" + std::string(body) + "}", scope, sources, "probe.xacro", silent);
    return { !out.has_value(), out.has_value() ? out->text : std::string() };
}

#ifdef MEIOS_TEST_HAS_YAML

// The document differential.py writes beside its probe, served here by name so both sides read
// the same bytes; upstream loads it as a value that contains itself.
inline constexpr std::string_view recursive_yaml = "a: &a [1, *a]\n";

class named_loader final : public meios::text_resource_loader::fetcher
{
public:
    meios::text_resource_result fetch(std::string_view, const std::filesystem::path &) override
    {
        return std::string(recursive_yaml);
    }
};

// A probe reading a document needs the module's own reader rather than the test double: the
// divergence it records belongs to the document reader, not to the expression grammar.
inline outcome evaluate_read(std::string_view expression)
{
    meios::eval_scope scope;
    scope.install_text_loader(meios::text_resource_loader{ std::make_unique<named_loader>() });
    scope.install_yaml_parser(meios::make_yaml_parser());
    meios::log_sink silent;
    meios::core_evaluator evaluator;
    const meios::value result = evaluator.eval(expression, scope, silent);
    return { evaluator.failed(), evaluator.failed() ? std::string() : rendered_as(result) };
}

// Which of this side's three entry points a probe is driven through: the grammar alone, a
// grammar reading a document, or the whole substitution including the span scan.
enum class driven { grammar, document, span };

// Spelled exactly as differential.py's own table spells them, so an id names the same rendered
// file whichever side wrote it.
struct probe
{
    driven through;
    std::string_view id;
    std::string_view expression;
};

inline constexpr std::array<probe, 13> divergence_probes{
    probe{ driven::grammar, "div_or_operand", "1 or 2" },
    probe{ driven::grammar, "div_and_operand", "2 and 3" },
    probe{ driven::document, "div_self_reference", "xacro.load_yaml('recursive.yaml')['a']" },
    probe{ driven::grammar, "div_dict_pair_sequence", "dict([('a', 1)])" },
    probe{ driven::grammar, "div_dict_mixed_arguments", "dict([('a', 1)], b=2)" },
    probe{ driven::grammar, "div_string_repetition", "'ab' * 3" },
    probe{ driven::grammar, "div_string_ordering", "'a' < 'b'" },
    probe{ driven::grammar, "div_string_split_whitespace", "'a b'.split()" },
    probe{ driven::grammar, "div_string_split_limit", "'a b c'.split(' ', 1)" },
    probe{ driven::grammar, "div_absolute_value", "abs(-3)" },
    probe{ driven::span, "div_span_closing_brace_in_literal", "'a}b'" },
    probe{ driven::span, "div_span_format_operator", "'%.3f' % 1.2345" },
    probe{ driven::span, "div_span_nested_expression", "${'1 + 2'}" }
};

inline outcome run_probe(const probe &one)
{
    if(one.through == driven::span)
        return evaluate_span(one.expression);
    if(one.through == driven::document)
        return evaluate_read(one.expression);
    return evaluate_bare(one.expression);
}

inline std::string observed_value(const probe &one)
{
    const outcome ran = run_probe(one);
    return ran.failed ? std::string("REFUSED") : ran.rendered;
}

inline std::string trimmed(std::string text)
{
    while(!text.empty() && (text.back() == '\n' || text.back() == '\r'))
        text.pop_back();
    return text;
}

#endif

}

#endif
