#include <meios/urdf.h>
#include <meios/model.h>
#include <meios/io.h>
#include <meios/xacro.h>

#include <meios/diagnostic/diagnostic_code.h>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace
{

struct entry
{
    meios::level lvl;
    meios::diagnostic_code code;
    std::string file;
    int line;
};

struct capture
{
    std::vector<entry> &entries;

    void operator()(meios::level lvl, const std::string &)
    {
        entries.push_back({ lvl, meios::diagnostic_code::unspecified, "", 0 });
    }

    void operator()(meios::level lvl, const meios::source_location &loc, const std::string &)
    {
        entries.push_back({ lvl, meios::diagnostic_code::unspecified, loc.file.string(), loc.line });
    }

    void operator()(meios::level lvl, meios::diagnostic_code code, const meios::source_location &loc,
                    const std::string &)
    {
        entries.push_back({ lvl, code, loc.file.string(), loc.line });
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
                              meios::strictness::strict,    path };
    meios::pod_recorder<meios::tree<double>> rec(cap, meios::topology_policy::fail);
    meios::basic_parser<meios::urdf_reader> parser(ctx);
    parser.parse(slurp(path), rec);
    return entries;
}

int error_count(const std::vector<entry> &entries)
{
    int n = 0;
    for(const entry &e : entries)
        if(e.lvl == meios::level::error)
            ++n;
    return n;
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

}

TEST_CASE("the pinned corpus parses the known-good/known-faulty pair with typed diagnostics",
          "[urdf][corpus]")
{
    int cases = 0;

    SECTION("known-good and known-faulty pair")
    {
        const std::filesystem::path good{ MEIOS_CORPUS_KNOWN_GOOD };
        INFO("known-good: " << good.string());
        REQUIRE(std::filesystem::exists(good));
        REQUIRE(error_count(load(good)) == 0);
        ++cases;

        const std::filesystem::path faulty{ MEIOS_CORPUS_FAULTY };
        INFO("known-faulty: " << faulty.string());
        REQUIRE(has_typed(load(faulty), meios::diagnostic_code::additional_root,
                          "faulty.urdf", MEIOS_CORPUS_FAULTY_LINE));
        ++cases;

#ifdef MEIOS_CORPUS_BREADTH
        const std::string breadth_dirs{ MEIOS_CORPUS_BREADTH_DIRS };
        std::size_t start = 0;
        while(start <= breadth_dirs.size())
        {
            const std::size_t bar = breadth_dirs.find('|', start);
            const std::string root =
                breadth_dirs.substr(start, bar == std::string::npos ? std::string::npos : bar - start);
            start = bar == std::string::npos ? breadth_dirs.size() + 1 : bar + 1;
            if(root.empty() || !std::filesystem::exists(root))
                continue;
            for(const std::filesystem::directory_entry &de :
                std::filesystem::recursive_directory_iterator(root))
            {
                if(!de.is_regular_file() || de.path().extension() != ".urdf")
                    continue;
                INFO("breadth: " << de.path().string());
                CHECK(error_count(load(de.path())) == 0);
                ++cases;
            }
        }
#endif

        REQUIRE(cases >= 2);
    }
}
