#ifdef MEIOS_CORPUS_EVAL_PYTHON
#include <meios/eval/python_evaluator.h>
#endif

#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>
#include <meios/xacro.h>

#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <string>
#include <vector>
#include <memory>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace
{

struct entry
{
    meios::diagnostic_code code;
    std::string file;
    int line;
};

struct capture
{
    std::vector<entry> &entries;

    void operator()(meios::level, const std::string &)
    {
        entries.push_back({ meios::diagnostic_code::unspecified, "", 0 });
    }

    void operator()(meios::level, const meios::source_location &loc, const std::string &)
    {
        entries.push_back({ meios::diagnostic_code::unspecified, loc.file.string(), loc.line });
    }

    void operator()(meios::level, meios::diagnostic_code code, const meios::source_location &loc,
                    const std::string &)
    {
        entries.push_back({ code, loc.file.string(), loc.line });
    }
};

std::string slurp(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::vector<entry> load(const std::filesystem::path &path)
{
    std::vector<entry> entries;
    meios::log_sink_f cap{ capture{ entries } };
    meios::source_stack sources{};
    meios::core_evaluator eval;
    meios::parse_context ctx{ sources,                     eval, cap, meios::missing_asset::warn,
                              meios::topology_policy::fail, meios::material_policy::warn,
                              meios::strictness::fail,      path };
    meios::pod_recorder<meios::tree<double>> rec(cap, meios::topology_policy::fail);
    meios::basic_parser<meios::urdf_reader> parser(ctx);
    parser.parse(slurp(path), rec);
    return entries;
}

bool has_typed(const std::vector<entry> &entries, meios::diagnostic_code code,
               const std::string &file_stem, int line)
{
    for(const entry &e : entries)
        if(e.code == code && e.line == line
           && std::filesystem::path(e.file).filename().string() == file_stem)
            return true;
    return false;
}

#ifdef MEIOS_CORPUS_EVAL_PYTHON
constexpr bool eval_python_linked = true;
#else
constexpr bool eval_python_linked = false;
#endif

struct document
{
    std::filesystem::path path;
    std::map<std::string, std::string> args;
    std::filesystem::path package_root;
    bool needs_python;
};

std::vector<std::string> split(const std::string &text, char sep)
{
    std::vector<std::string> fields;
    for(std::size_t start = 0; start <= text.size();)
    {
        const std::size_t at = std::min(text.find(sep, start), text.size());
        fields.push_back(text.substr(start, at - start));
        start = at + 1;
    }
    return fields;
}

std::map<std::string, std::string> arg_map(const std::string &spec)
{
    std::map<std::string, std::string> args;
    for(const std::string &pair : split(spec, ' '))
    {
        const std::size_t split_at = pair.find('=');
        if(split_at != std::string::npos)
            args.emplace(pair.substr(0, split_at), pair.substr(split_at + 1));
    }
    return args;
}

// Four fields per record, joined with '|': path, expansion arguments, evaluator,
// package root. The list is named in cmake/corpus.cmake; nothing here globs.
std::vector<document> documents()
{
    const std::vector<std::string> fields = split(MEIOS_CORPUS_DOCUMENTS, '|');
    std::vector<document> docs;
    for(std::size_t at = 0; at + 3 < fields.size(); at += 4)
        docs.push_back({ fields[at], arg_map(fields[at + 1]), fields[at + 3],
                         fields[at + 2] == "python" });
    return docs;
}

meios::load_options options_for(const document &doc)
{
    meios::load_options opts;
    opts.on_missing = meios::missing_asset::warn;
    opts.materials = meios::material_policy::warn;
    opts.args = doc.args;
    if(!doc.package_root.empty())
        opts.package_roots.push_back(doc.package_root);
#ifdef MEIOS_CORPUS_EVAL_PYTHON
    if(doc.needs_python)
        opts.backend = std::make_shared<meios::evaluator_handle>(meios::python_evaluator{});
#endif
    return opts;
}

}

TEST_CASE("the pinned corpus parses the known-good/known-faulty pair with typed diagnostics",
          "[urdf][corpus]")
{
    const std::filesystem::path good{ MEIOS_CORPUS_KNOWN_GOOD };
    INFO("known-good: " << good.string());
    REQUIRE(std::filesystem::exists(good));
    REQUIRE(meios::load(good, options_for(document{ good, {}, {}, false })).has_value());

    const std::filesystem::path faulty{ MEIOS_CORPUS_FAULTY };
    INFO("known-faulty: " << faulty.string());
    REQUIRE(has_typed(load(faulty), meios::diagnostic_code::additional_root,
                      "faulty.urdf", MEIOS_CORPUS_FAULTY_LINE));
}

TEST_CASE("every named top-level corpus document loads with no error-level diagnostic",
          "[urdf][corpus]")
{
    const std::vector<document> docs = documents();
    REQUIRE(docs.size() == static_cast<std::size_t>(MEIOS_CORPUS_DOCUMENT_COUNT));

    std::size_t loaded = 0;
    std::size_t skipped = 0;
    for(const document &doc : docs)
    {
        INFO("document: " << doc.path.string());
        REQUIRE(std::filesystem::exists(doc.path));
        if(doc.needs_python && !eval_python_linked)
        {
            ++skipped;
            continue;
        }
        const meios::expected<meios::load_result, meios::load_error> result =
            meios::load(doc.path, options_for(doc));
        if(!result.has_value())
            FAIL("refused " << doc.path.string() << ": " << result.error().message);
        ++loaded;
    }

    // Reported unconditionally: a leg that covers less than it appears to must say so
    // in its own output rather than pass quietly.
    WARN("corpus documents: " << loaded << " loaded, " << skipped
                              << " skipped for want of the python evaluator");
    REQUIRE(loaded + skipped == docs.size());
}
