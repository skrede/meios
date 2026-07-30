#ifndef HPP_GUARD_MEIOS_TEST_ACQUISITION_SCRIPT_H
#define HPP_GUARD_MEIOS_TEST_ACQUISITION_SCRIPT_H

#include "meios/io/text_reader_operations.h"

#include <span>
#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <algorithm>
#include <filesystem>
#include <system_error>

namespace acquisition_test
{

class error_category final : public std::error_category
{
public:
    const char *name() const noexcept override
    {
        return "acquisition-test";
    }

    std::string message(int value) const override
    {
        return std::to_string(value);
    }
};

inline const error_category category;

struct reader_script
{
    reader_script()
            : status(std::filesystem::file_type::regular)
            , status_error()
            , open_error()
            , read_error()
            , close_error()
            , content(R"(<robot name="checked"><link name="base"/></robot>)")
            , chunks()
            , calls()
            , offset(0)
            , next(0)
            , request(0)
            , over_transfer(0)
            , fail_open(false)
            , fail_read(false)
            , fail_close(false)
            , closes(0)
    {
    }

    std::filesystem::file_status status;
    std::error_code status_error;
    std::error_code open_error;
    std::error_code read_error;
    std::error_code close_error;
    std::string content;
    std::vector<std::string> chunks;
    std::vector<std::string> calls;
    std::size_t offset;
    std::size_t next;
    std::size_t request;
    std::size_t over_transfer;
    bool fail_open;
    bool fail_read;
    bool fail_close;
    int closes;
};

class scripted_file final : public meios::detail::text_file
{
public:
    explicit scripted_file(reader_script &script)
            : m_script(script)
            , m_closed(false)
    {
    }

    ~scripted_file() override
    {
        if(!m_closed)
            (void)close();
    }

    std::size_t read(std::span<char> buffer) noexcept override
    {
        m_script.calls.emplace_back("read");
        m_script.request = buffer.size();
        if(m_script.over_transfer != 0)
            return buffer.size() + m_script.over_transfer;
        if(!m_script.chunks.empty())
            return read_chunk(buffer);
        return read_content(buffer);
    }

    bool failed() const noexcept override
    {
        m_script.calls.emplace_back("error");
        return m_script.fail_read || bool(m_script.read_error);
    }

    std::error_code native_error() const noexcept override
    {
        return m_script.read_error ? m_script.read_error : m_script.close_error;
    }

    bool close() noexcept override
    {
        m_closed = true;
        ++m_script.closes;
        m_script.calls.emplace_back("close");
        return !m_script.fail_close && !m_script.close_error;
    }

private:
    reader_script &m_script;
    bool m_closed;

    std::size_t read_chunk(std::span<char> buffer) noexcept
    {
        if(m_script.next == m_script.chunks.size())
            return 0;
        const std::string &chunk = m_script.chunks[m_script.next++];
        const std::size_t count  = std::min(buffer.size(), chunk.size());
        std::copy_n(chunk.data(), count, buffer.data());
        return count;
    }

    std::size_t read_content(std::span<char> buffer) noexcept
    {
        if(m_script.offset == m_script.content.size())
            return 0;
        const std::size_t count = std::min(buffer.size(), m_script.content.size() - m_script.offset);
        std::copy_n(m_script.content.data() + m_script.offset, count, buffer.data());
        m_script.offset += count;
        return count;
    }
};

class scripted_operations final : public meios::detail::text_reader_operations
{
public:
    explicit scripted_operations(reader_script &script)
            : m_script(script)
    {
    }

    meios::detail::text_status_result status(const std::filesystem::path &) const noexcept override
    {
        m_script.calls.emplace_back("status");
        if(m_script.status_error)
            return meios::unexpected<std::error_code>(m_script.status_error);
        return m_script.status;
    }

    meios::detail::text_file_result open(const std::filesystem::path &) const noexcept override
    {
        m_script.calls.emplace_back("open");
        if(m_script.fail_open || m_script.open_error)
            return meios::unexpected<std::error_code>(m_script.open_error);
        return std::unique_ptr<meios::detail::text_file>(std::make_unique<scripted_file>(m_script));
    }

    meios::detail::text_open_result open_under(const std::filesystem::path &, const std::filesystem::path &) const noexcept override
    {
        m_script.calls.emplace_back("open_under");
        if(m_script.status_error)
            return refused(meios::text_read_failure_kind::status, meios::operation_kind::status, m_script.status_error);
        if(!std::filesystem::is_regular_file(m_script.status))
            return refused(meios::text_read_failure_kind::non_regular, meios::operation_kind::status, {});
        if(m_script.fail_open || m_script.open_error)
            return refused(meios::text_read_failure_kind::open, meios::operation_kind::open, m_script.open_error);
        return std::unique_ptr<meios::detail::text_file>(std::make_unique<scripted_file>(m_script));
    }

private:
    reader_script &m_script;

    static meios::detail::text_open_result refused(meios::text_read_failure_kind kind, meios::operation_kind operation, std::error_code error)
    {
        if(!error && kind != meios::text_read_failure_kind::non_regular)
            error = make_error_code(std::errc::io_error);
        return meios::unexpected<meios::text_read_failure>({kind, {operation, error}});
    }
};

}

#endif
