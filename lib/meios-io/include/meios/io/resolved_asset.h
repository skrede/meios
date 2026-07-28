#ifndef HPP_GUARD_MEIOS_IO_RESOLVED_ASSET_H
#define HPP_GUARD_MEIOS_IO_RESOLVED_ASSET_H

#include <utility>
#include <filesystem>

namespace meios
{

// A located asset as a resolvable filesystem path, valid for exactly as long as the
// source that produced it lives.
class resolved_asset
{
public:
    explicit resolved_asset(std::filesystem::path path) : m_path(std::move(path)) {}

    const std::filesystem::path &path() const noexcept { return m_path; }

private:
    std::filesystem::path m_path;
};

}

#endif
