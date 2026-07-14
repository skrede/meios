#ifndef HPP_GUARD_MEIOS_IO_RESOLVED_ASSET_H
#define HPP_GUARD_MEIOS_IO_RESOLVED_ASSET_H

#include <span>
#include <memory>
#include <cstddef>
#include <utility>
#include <variant>
#include <optional>
#include <filesystem>
#include <system_error>

namespace meios
{

// Move-only pull stream over an asset's bytes. read() fills the span and returns
// the count written; 0 means end of stream. A copyable callable cannot own a
// move-only underlying reader, and std::move_only_function is C++23, so the
// interface is a unique_ptr to an implementor rather than a stored callable.
class byte_reader
{
public:
    struct puller
    {
        puller() = default;
        puller(const puller &) = default;
        puller &operator=(const puller &) = default;
        puller(puller &&) = default;
        puller &operator=(puller &&) = default;
        virtual ~puller() = default;

        virtual std::size_t read(std::span<std::byte> out) = 0;
    };

    explicit byte_reader(std::unique_ptr<puller> impl) : m_impl(std::move(impl)) {}

    byte_reader(byte_reader &&) noexcept = default;
    byte_reader &operator=(byte_reader &&) noexcept = default;
    byte_reader(const byte_reader &) = delete;
    byte_reader &operator=(const byte_reader &) = delete;

    ~byte_reader() = default;

    bool valid() const noexcept { return m_impl != nullptr; }

    std::size_t read(std::span<std::byte> out) { return m_impl->read(out); }

private:
    std::unique_ptr<puller> m_impl;
};

// Owns a materialized temp file for its lifetime and unlinks it on destruction.
// The error_code overload of remove is used so the destructor never throws; a
// swap-based move keeps the retiring path alive on the moved-from guard so a
// move-assignment target's prior file is still unlinked exactly once.
class temp_file_guard
{
public:
    temp_file_guard() = default;
    explicit temp_file_guard(std::filesystem::path path) : m_path(std::move(path)) {}

    temp_file_guard(temp_file_guard &&other) noexcept { m_path.swap(other.m_path); }

    temp_file_guard &operator=(temp_file_guard &&other) noexcept
    {
        m_path.swap(other.m_path);
        return *this;
    }

    temp_file_guard(const temp_file_guard &) = delete;
    temp_file_guard &operator=(const temp_file_guard &) = delete;

    ~temp_file_guard()
    {
        if(m_path.empty())
            return;
        std::error_code ec;
        std::filesystem::remove(m_path, ec);
    }

    const std::filesystem::path &path() const noexcept { return m_path; }

private:
    std::filesystem::path m_path;
};

// A located asset delivered either as a resolvable filesystem path or as a byte
// stream. It is move-only because it owns the optional temp-file lifetime.
class resolved_asset
{
public:
    explicit resolved_asset(std::filesystem::path path)
        : m_guard(std::nullopt), m_content(std::move(path))
    {
    }

    explicit resolved_asset(byte_reader reader)
        : m_guard(std::nullopt), m_content(std::move(reader))
    {
    }

    resolved_asset(std::filesystem::path path, temp_file_guard guard)
        : m_guard(std::move(guard)), m_content(std::move(path))
    {
    }

    resolved_asset(resolved_asset &&) noexcept = default;
    resolved_asset &operator=(resolved_asset &&) noexcept = default;
    resolved_asset(const resolved_asset &) = delete;
    resolved_asset &operator=(const resolved_asset &) = delete;

    ~resolved_asset() = default;

    bool holds_path() const noexcept
    {
        return std::holds_alternative<std::filesystem::path>(m_content);
    }

    bool holds_bytes() const noexcept
    {
        return std::holds_alternative<byte_reader>(m_content);
    }

    const std::filesystem::path &path() const
    {
        return std::get<std::filesystem::path>(m_content);
    }

    byte_reader &bytes() { return std::get<byte_reader>(m_content); }

private:
    std::optional<temp_file_guard> m_guard;
    std::variant<std::filesystem::path, byte_reader> m_content;
};

}

#endif
