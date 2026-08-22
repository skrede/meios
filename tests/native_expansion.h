#ifndef HPP_GUARD_MEIOS_TESTS_NATIVE_EXPANSION_H
#define HPP_GUARD_MEIOS_TESTS_NATIVE_EXPANSION_H

#include "corpus_record.h"

#include "meios/urdf/yaml_resource.h"

#include <meios/io.h>
#include <meios/xacro.h>

#ifdef MEIOS_TEST_HAS_YAML
    #include <meios/yaml/parser.h>
#endif

#include <string>
#include <vector>
#include <utility>
#include <fstream>
#include <iterator>
#include <optional>
#include <filesystem>

namespace corpus
{

inline std::string bytes_of(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string{ std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
}

// The expansion the load path runs, stopping one stage earlier: a model carries neither the
// flattened text nor a record of what the evaluation exercised, and a comparison against
// upstream's own render needs the first while a ledger row needs the second. Everything the load
// path installs is installed here too -- the argument seeding, the document-relative text loader
// over the package roots, and the auxiliary-document reader -- so what is observed is the load a
// consumer gets rather than a reduced one.
inline std::optional<meios::expansion> expanded(const document &doc)
{
    meios::log_sink silent;
    std::vector<std::filesystem::path> roots;
    if(!doc.package_root.empty())
        roots.push_back(doc.package_root);
    meios::source_stack sources;
    for(const std::filesystem::path &root : roots)
        sources.push_back(meios::source_handle(meios::directory_source(root, silent)));
    meios::eval_scope scope;
    for(const std::pair<const std::string, std::string> &arg : doc.args)
        scope.set(arg.first,
                  meios::value{ meios::detail::strip_authored_markers(arg.second) });
    scope.install_text_loader(meios::detail::make_yaml_text_loader(sources, roots, silent));
#ifdef MEIOS_TEST_HAS_YAML
    scope.install_yaml_parser(meios::make_yaml_parser());
#endif
    meios::expected<meios::expansion, meios::expansion_error> out =
        meios::expand(bytes_of(doc.path), scope, sources, doc.path, meios::expansion_limits{},
                      silent);
    if(!out)
        return std::nullopt;
    return std::move(out.value());
}

}

#endif
