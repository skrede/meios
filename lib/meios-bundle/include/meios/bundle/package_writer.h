#ifndef HPP_GUARD_MEIOS_BUNDLE_PACKAGE_WRITER_H
#define HPP_GUARD_MEIOS_BUNDLE_PACKAGE_WRITER_H

#include "meios/diagnostic/log_sink.h"

#include <functional>
#include <filesystem>

namespace meios
{

// Writes a self-contained package rooted at a single directory: the inverse of
// bundle_source, creating dirs and copying assets into the root rather than
// resolving out of it. The write/emit surface lands with the folder-writer body.
class package_writer
{
public:
    package_writer(std::filesystem::path root, log_sink &log);

private:
    std::filesystem::path m_root;
    std::reference_wrapper<log_sink> m_log;
};

}

#endif
