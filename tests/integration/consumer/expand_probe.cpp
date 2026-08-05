#include "consumer_probe.h"

#include <meios/io.h>
#include <meios/xacro.h>

#include <string>
#include <iostream>
#include <filesystem>

namespace consumer
{

namespace
{

using expansion_result = meios::expected<meios::expansion, meios::expansion_error>;
using substitution_result = meios::expected<meios::substitution, meios::expansion_error>;

int refuse(const char *what)
{
    std::cerr << what << '\n';
    return 1;
}

// The source stack outlives every read of what the expansion produced: it is constructed
// here and destroyed only once the probes below have finished with their results.
int run_expansion(meios::source_stack &sources, const std::filesystem::path &document)
{
    const char *good = "<robot name=\"probe\" xmlns:xacro=\"http://www.ros.org/wiki/xacro\">"
                       "<xacro:property name=\"count\" value=\"3\"/>"
                       "<link name=\"link_${count*2}\"/></robot>";
    meios::eval_scope scope;
    meios::log_sink log;
    const expansion_result expanded =
        meios::expand(good, scope, sources, document, meios::expansion_limits{}, log);
    if(!expanded)
        return refuse("the installed expansion entry point refused a well-formed document");
    if(expanded->document.find("link_6") == std::string::npos)
        return refuse("the expanded document does not carry the resolved expression");
    std::cout << "the installed package expanded a document through the value arm ("
              << expanded->document.size() << " bytes)\n";
    return 0;
}

int run_refused_expansion(meios::source_stack &sources, const std::filesystem::path &document)
{
    meios::eval_scope scope;
    meios::log_sink log;
    const expansion_result refused =
        meios::expand("<robot name=\"broken\"><link name=\"a\"</robot>", scope, sources, document,
                      meios::expansion_limits{}, log);
    if(refused.has_value())
        return refuse("a malformed document was expanded rather than refused");
    if(refused.error().code == meios::diagnostic_code::unspecified)
        return refuse("the terminal record crossed the install boundary without a code");
    if(refused.error().loc.file != document)
        return refuse("the terminal record crossed the install boundary without its document");
    std::cout << "a refused expansion crossed the install boundary (code="
              << meios::to_string(refused.error().code)
              << ", at=" << meios::to_string(refused.error().loc) << ")\n";
    return 0;
}

int run_substitution(meios::source_stack &sources, const std::filesystem::path &document)
{
    meios::eval_scope scope;
    meios::log_sink log;
    const substitution_result resolved =
        meios::substitute("${1+1}", scope, sources, document, log);
    if(!resolved || resolved->text != "2")
        return refuse("the installed substitution entry point did not resolve an expression");
    const substitution_result refused =
        meios::substitute("${1+1", scope, sources, document, log);
    if(refused.has_value())
        return refuse("an unterminated span was substituted rather than refused");
    if(refused.error().code != meios::diagnostic_code::unterminated_substitution)
        return refuse("a refused substitution crossed the install boundary without its code");
    std::cout << "the installed substitution surface answered both arms (code="
              << meios::to_string(refused.error().code) << ")\n";
    return 0;
}

}

int run_expansions()
{
    meios::source_stack sources{};
    const std::filesystem::path document = std::filesystem::current_path() / "probe.urdf.xacro";

    if(const int rc = run_expansion(sources, document))
        return rc;
    if(const int rc = run_refused_expansion(sources, document))
        return rc;
    return run_substitution(sources, document);
}

}
