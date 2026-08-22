#ifndef HPP_GUARD_MEIOS_IO_RESOLVED_ASSET_H
#define HPP_GUARD_MEIOS_IO_RESOLVED_ASSET_H

#include <utility>
#include <optional>
#include <filesystem>

namespace meios
{

class resolved_asset
{
public:
    explicit resolved_asset(std::filesystem::path path)
            : m_path(std::move(path))
            , m_source_root()
            , m_source_relative()
    {
    }

    resolved_asset(std::filesystem::path path, std::filesystem::path source_root, std::filesystem::path source_relative)
            : m_path(std::move(path))
            , m_source_root(std::move(source_root))
            , m_source_relative(std::move(source_relative))
    {
    }

    const std::filesystem::path &path() const noexcept
    {
        return m_path;
    }

    const std::optional<std::filesystem::path> &source_root() const noexcept
    {
        return m_source_root;
    }

    const std::optional<std::filesystem::path> &source_relative() const noexcept
    {
        return m_source_relative;
    }

private:
    std::filesystem::path m_path;
    std::optional<std::filesystem::path> m_source_root;
    std::optional<std::filesystem::path> m_source_relative;
};

}

#endif
