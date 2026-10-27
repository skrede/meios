#ifndef HPP_GUARD_MEIOS_UNIT_ZIP_WRITER_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_ZIP_WRITER_FIXTURE_H

#include <miniz.h>

#include <map>
#include <string>
#include <cstddef>
#include <fstream>
#include <filesystem>

namespace zip_fixture
{

inline std::filesystem::path scratch_root(const std::string &suffix)
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / ("meios_zip_" + suffix);
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

inline std::filesystem::path write_file(const std::filesystem::path &path, const std::string &text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << text;
    return path;
}

inline std::map<std::string, std::string> read_archive(const std::filesystem::path &path)
{
    std::map<std::string, std::string> out;
    mz_zip_archive zip;
    mz_zip_zero_struct(&zip);
    if(!mz_zip_reader_init_file(&zip, path.string().c_str(), 0))
        return out;
    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    for(mz_uint i = 0; i < count; ++i)
    {
        mz_zip_archive_file_stat stat;
        if(!mz_zip_reader_file_stat(&zip, i, &stat))
            continue;
        std::size_t size = 0;
        void *raw = mz_zip_reader_extract_to_heap(&zip, i, &size, 0);
        out.emplace(stat.m_filename, std::string(static_cast<const char *>(raw), size));
        mz_free(raw);
    }
    mz_zip_reader_end(&zip);
    return out;
}

}

#endif
