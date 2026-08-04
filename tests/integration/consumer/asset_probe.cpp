#include "consumer_probe.h"

#include <meios/io.h>

#include <meios/bundle/asset_bytes.h>

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <system_error>

namespace consumer
{

namespace
{

int refuse(const char *what)
{
    std::cerr << what << '\n';
    return 1;
}

std::filesystem::path seed(const std::filesystem::path &root, const std::string &text)
{
    std::filesystem::create_directories(root);
    const std::filesystem::path file = root / "mesh.obj";
    std::ofstream out(file, std::ios::binary);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    return file;
}

int run_published_reader(const std::filesystem::path &root, const std::string &text)
{
    const meios::text_read_result read = meios::read_text_file_under(root, "mesh.obj");
    if(!read || *read != text)
        return refuse("the published root-relative reader did not answer the text it was given");
    std::cout << "the installed package declares the root-relative reader (" << read->size() << " bytes)\n";
    return 0;
}

int run_bundle_helper(const std::filesystem::path &file, const std::string &text)
{
    std::vector<diagnostic_record> records;
    meios::log_sink_f<diagnostic_lift> sink{ diagnostic_lift{ records } };
    const meios::text_read_result read = meios::read_asset_text(meios::resolved_asset{ file }, sink);
    if(!read || *read != text)
        return refuse("the public bundle read helper did not answer a regular file's text");
    if(!records.empty())
        return refuse("a successful asset read reported a diagnostic");
    std::cout << "the installed public bundle helper read a resolved asset (" << read->size() << " bytes)\n";
    return 0;
}

int run_refused_directory(const std::filesystem::path &root)
{
    std::vector<diagnostic_record> records;
    meios::log_sink_f<diagnostic_lift> sink{ diagnostic_lift{ records } };
    if(meios::read_asset_text(meios::resolved_asset{ root }, sink).has_value())
        return refuse("a directory handed to the public bundle helper was read rather than refused");
    if(records.size() != 1)
        return refuse("a refused asset read did not report exactly one diagnostic");
    const diagnostic_record &only = records.front();
    if(only.lvl != meios::level::error || only.code != meios::diagnostic_code::cannot_open)
        return refuse("a refused asset read did not cross the install boundary with its code");
    if(!only.cause)
        return refuse("no structured cause crossed the install boundary behind a refused read");
    std::cout << "a refused asset read crossed the install boundary (code=" << meios::to_string(only.code)
              << ", operation=" << meios::to_string(only.cause->operation)
              << ", native=" << only.cause->native.value() << ")\n";
    return 0;
}

}

int run_asset_reads()
{
    const std::string text = "mtllib wood.mtl\n";
    const std::filesystem::path root = std::filesystem::current_path() / "asset_probe_tree";
    const std::filesystem::path file = seed(root, text);

    if(const int rc = run_published_reader(root, text))
        return rc;
    if(const int rc = run_bundle_helper(file, text))
        return rc;
    if(const int rc = run_refused_directory(root))
        return rc;

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    return 0;
}

}
