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
    std::filesystem::file_status status{std::filesystem::file_type::regular};
    std::error_code status_error;
    std::error_code open_error;
    std::error_code read_error;
    std::error_code close_error;
    std::string content{R"(<robot name="checked"><link name="base"/></robot>)"};
    std::vector<std::string> calls;
    std::size_t offset{0};
    int closes{0};
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
        if(m_script.offset == m_script.content.size())
            return 0;
        const std::size_t count = std::min(buffer.size(), m_script.content.size() - m_script.offset);
        std::copy_n(m_script.content.data() + m_script.offset, count, buffer.data());
        m_script.offset += count;
        return count;
    }

    bool failed() const noexcept override
    {
        return bool(m_script.read_error) && m_script.offset != 0;
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
        return !m_script.close_error;
    }

private:
    reader_script &m_script;
    bool m_closed;
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
        if(m_script.open_error)
            return meios::unexpected<std::error_code>(m_script.open_error);
        return std::unique_ptr<meios::detail::text_file>(new scripted_file(m_script));
    }

private:
    reader_script &m_script;
};

}

#endif
