#include "model_facts.h"
#include "corpus_paths.h"

#include <meios/urdf.h>
#include <meios/model.h>

#include <string>
#include <vector>
#include <cstddef>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <exception>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace
{

#ifdef _WIN32
constexpr char path_separator = ';';
constexpr std::string_view interpreter_names[] = { "python.exe", "python3.exe", "py.exe" };
#else
constexpr char path_separator = ':';
constexpr std::string_view interpreter_names[] = { "python", "python3" };
#endif

int refuse(const std::string &what)
{
    std::cerr << what << '\n';
    return 1;
}

// A versioned spelling extends a bare one with a dot and the version, on either platform:
// python3.13 where executables carry no extension, python3.13.exe where they do.
bool names_interpreter(const std::string &entry)
{
    for(const std::string_view name : interpreter_names)
        if(entry == name)
            return true;
    return entry.rfind("python2.", 0) == 0 || entry.rfind("python3.", 0) == 0;
}

std::string interpreter_in(const std::string &dir)
{
    std::error_code unreadable;
    for(const std::filesystem::directory_entry &entry :
        std::filesystem::directory_iterator(dir, unreadable))
    {
        const std::string name = entry.path().filename().string();
        if(names_interpreter(name))
            return (std::filesystem::path(dir) / name).string();
    }
    return {};
}

std::string interpreter_on(const std::string &path)
{
    for(std::string rest = path; !rest.empty();)
    {
        const std::size_t split_at = rest.find(path_separator);
        const std::string dir = rest.substr(0, split_at);
        rest = split_at == std::string::npos ? std::string{} : rest.substr(split_at + 1);
        if(dir.empty())
            continue;
        const std::string found = interpreter_in(dir);
        if(!found.empty())
            return found;
    }
    return {};
}

// The claim is defensible only against a path that is set and empty, and it is decided on that
// emptiness rather than on what a scan found. An unset path is not an absence of candidates:
// POSIX execvp falls back to confstr(_CS_PATH) when PATH is not in the environment, so a system
// interpreter stays spawnable while a scan of nothing reports nothing. The scan survives because
// it is what makes a refusal actionable -- it names the directory and the executable it matched.
int refuse_reachable_interpreter()
{
    const char *path = std::getenv("PATH");
    if(path == nullptr)
        return refuse("the process path is unset, so the system default path still resolves an "
                      "interpreter and this run cannot state that the load needed none");
    const std::string rest = path;
    if(rest.empty())
        return 0;
    const std::string found = interpreter_on(rest);
    if(found.empty())
        return refuse("the process path is not empty, so this run cannot state that the load "
                      "needed no interpreter; it names " + rest);
    return refuse("an interpreter is reachable at " + found
                  + ", so this run cannot state that the load needed none");
}

// The arming fact and the fact under inspection come from different sources on purpose: an
// invocation that lost the environment emptying the path is left armed against a full path, so
// the leg reddens instead of quietly not existing.
bool claims_no_interpreter(int argc, char **argv)
{
    for(int at = 1; at < argc; ++at)
        if(std::string_view(argv[at]) == "--no-interpreter")
            return true;
    return std::getenv("MEIOS_CONSUMER_NO_INTERPRETER") != nullptr;
}

int load_and_compare(const std::vector<oracle::row> &rows)
{
    meios::load_options opts;
    opts.on_missing = meios::missing_asset::warn;
    opts.materials = meios::material_policy::warn;
    opts.args.emplace(consumer::variant_argument, consumer::variant);
    opts.package_roots.push_back(consumer::pinned_package_root);

    const meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load(consumer::pinned_document, opts);
    if(!loaded)
    {
        std::ostringstream refusal;
        refusal << "load() refused " << consumer::pinned_document << ": ("
                << meios::to_string(loaded.error().code) << ") " << loaded.error().message;
        return refuse(refusal.str());
    }

    const meios::model<double> &robot = loaded->robot;
    if(const int rc = consumer::check_structure(robot, rows))
        return rc;
    if(const int rc = consumer::check_limits(robot, rows))
        return rc;
    if(const int rc = consumer::check_assets(robot, rows))
        return rc;

    std::cout << "loaded " << consumer::pinned_document << " as " << consumer::variant
              << " (links=" << robot.links.size() << ", joints=" << robot.joints.size()
              << ") and matched " << rows.size() << " recorded facts\n";
    return 0;
}

}

int main(int argc, char **argv)
{
    if(claims_no_interpreter(argc, argv))
        if(const int rc = refuse_reachable_interpreter())
            return rc;

    try
    {
        return load_and_compare(oracle::load_rows(consumer::pinned_record));
    }
    catch(const std::exception &unreadable)
    {
        return refuse(std::string("the committed record ") + consumer::pinned_record
                      + " yielded nothing to compare against: " + unreadable.what());
    }
}
