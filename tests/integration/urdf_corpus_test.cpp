#include "../model_facts.h"
#include "../corpus_record.h"

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

meios::load_options options_for(const corpus::document &doc)
{
    if(doc.backend == corpus::evaluator::unknown)
        FAIL("unrecognized evaluator '" << doc.spelling << "' for " << doc.path.string());
    meios::load_options opts;
    opts.on_missing = meios::missing_asset::warn;
    opts.materials = meios::material_policy::warn;
    opts.args = doc.args;
    if(!doc.package_root.empty())
        opts.package_roots.push_back(doc.package_root);
#ifdef MEIOS_CORPUS_EVAL_PYTHON
    if(doc.backend == corpus::evaluator::python)
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
    REQUIRE(meios::load(good, options_for(corpus::document{ good, {}, {},
                                                           corpus::evaluator::core, "core" }))
                .has_value());

    const std::filesystem::path faulty{ MEIOS_CORPUS_FAULTY };
    INFO("known-faulty: " << faulty.string());
    REQUIRE(has_typed(load(faulty), meios::diagnostic_code::additional_root,
                      "faulty.urdf", MEIOS_CORPUS_FAULTY_LINE));
}

TEST_CASE("a document sharing ur5e's variant plus a second argument keys distinctly",
          "[urdf][corpus]")
{
    const corpus::document doc{ "ur.urdf.xacro",
                                { { "ur_type", "ur5e" }, { "unmeasured_probe_arg", "true" } }, "",
                                corpus::evaluator::native, "native" };
    REQUIRE(corpus::record_for(doc).empty());
}

TEST_CASE("every named top-level corpus document loads with no error-level diagnostic",
          "[urdf][corpus]")
{
    const std::vector<corpus::document> docs = corpus::documents(MEIOS_CORPUS_DOCUMENTS);
    REQUIRE(docs.size() == static_cast<std::size_t>(MEIOS_CORPUS_DOCUMENT_COUNT));

    std::size_t loaded = 0;
    std::size_t skipped = 0;
    for(const corpus::document &doc : docs)
    {
        INFO("document: " << doc.path.string());
        REQUIRE(std::filesystem::exists(doc.path));
        // The only skippable document is one wanting an absent optional backend. Nothing about
        // the built-in evaluator is optional, so a document classified for it is never skipped.
        if(doc.backend == corpus::evaluator::python && !eval_python_linked)
        {
            ++skipped;
            continue;
        }
        const meios::expected<meios::load_result, meios::load_error> result =
            meios::load(doc.path, options_for(doc));
        if(!result.has_value())
            FAIL("refused " << doc.path.string() << ": " << result.error().message);
        if(doc.backend == corpus::evaluator::native)
        {
            const std::string record = corpus::record_for(doc);
            INFO("measured facts: " << record);
            REQUIRE_FALSE(record.empty());
            facts::check_model(oracle::load_rows(record), result->robot, doc.package_root);
        }
        ++loaded;
    }

    // Reported unconditionally: a leg that covers less than it appears to must say so
    // in its own output rather than pass quietly.
    WARN("corpus documents: " << loaded << " loaded, " << skipped
                              << " skipped for want of the optional backend");
    REQUIRE(loaded + skipped == docs.size());
}
