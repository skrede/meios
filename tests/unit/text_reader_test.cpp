#include <meios/io/text_reader.h>

#include "meios/io/text_reader_operations.h"

#include <catch2/catch_test_macros.hpp>

#include <span>
#include <memory>
#include <string>
#include <vector>
#include <cstddef>
#include <algorithm>
#include <filesystem>
#include <system_error>
namespace
{
class custom_error_category final : public std::error_category
{
public:
    const char *name() const noexcept override
    {
        return "reader-test";
    }

    std::string message(int value) const override
    {
        return std::to_string(value);
    }
};
const custom_error_category category;
struct script
{
    std::filesystem::file_status status;
    std::error_code status_error;
    std::error_code open_error;
    std::error_code read_error;
    std::error_code close_error;
    std::vector<std::string> chunks;
    std::vector<std::string> calls;
    std::size_t request;
    std::size_t next;
    int closes;
};
const script regular{std::filesystem::file_status(std::filesystem::file_type::regular), {}, {}, {}, {}, {}, {}, 0, 0, 0};
class scripted_text_file final : public meios::detail::text_file
{
public:
    explicit scripted_text_file(script &state)
            : m_state(state)
            , m_closed(false)
    {
    }

    ~scripted_text_file() override
    {
        if(!m_closed)
            (void)close();
    }

    std::size_t read(std::span<char> buffer) noexcept override
    {
        m_state.calls.emplace_back("read");
        m_state.request = buffer.size();
        if(m_state.next == m_state.chunks.size())
            return 0;
        const std::string &chunk = m_state.chunks[m_state.next++];
        std::copy(chunk.begin(), chunk.end(), buffer.begin());
        return chunk.size();
    }

    bool failed() const noexcept override
    {
        m_state.calls.emplace_back("error");
        return bool(m_state.read_error) && m_state.next > 0;
    }

    std::error_code native_error() const noexcept override
    {
        return m_state.read_error ? m_state.read_error : m_state.close_error;
    }

    bool close() noexcept override
    {
        m_closed = true;
        ++m_state.closes;
        m_state.calls.emplace_back("close");
        return !m_state.close_error;
    }

private:
    script &m_state;
    bool m_closed;
};
class scripted_operations final : public meios::detail::text_reader_operations
{
public:
    explicit scripted_operations(script &state)
            : m_state(state)
    {
    }

    meios::detail::text_status_result status(const std::filesystem::path &) const noexcept override
    {
        m_state.calls.emplace_back("status");
        if(m_state.status_error)
            return meios::unexpected<std::error_code>(m_state.status_error);
        return m_state.status;
    }

    meios::detail::text_file_result open(const std::filesystem::path &) const noexcept override
    {
        m_state.calls.emplace_back("open");
        if(m_state.open_error)
            return meios::unexpected<std::error_code>(m_state.open_error);
        return std::unique_ptr<meios::detail::text_file>(new scripted_text_file(m_state));
    }

private:
    script &m_state;
};
meios::text_read_result run(script &state)
{
    scripted_operations operations{state};
    return meios::detail::read_text_file("scripted", operations);
}

void require_failure(const meios::text_read_result &result, meios::text_read_failure_kind kind, meios::operation_kind operation, int value)
{
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().kind == kind);
    REQUIRE(result.error().cause.operation == operation);
    REQUIRE(&result.error().cause.native.category() == &category);
    REQUIRE(result.error().cause.native.value() == value);
}
}
TEST_CASE("status and non-regular refusals stop before open", "[text_reader]")
{
    script failed       = regular;
    failed.status_error = {11, category};
    require_failure(run(failed), meios::text_read_failure_kind::status, meios::operation_kind::status, 11);
    REQUIRE(failed.calls == std::vector<std::string>{"status"});
    script non_regular                   = regular;
    non_regular.status                   = std::filesystem::file_status(std::filesystem::file_type::directory);
    const meios::text_read_result result = run(non_regular);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error().kind == meios::text_read_failure_kind::non_regular);
    REQUIRE_FALSE(result.error().cause.native);
    REQUIRE(non_regular.calls == std::vector<std::string>{"status"});
}
TEST_CASE("open refusal stops before transfer", "[text_reader]")
{
    script state     = regular;
    state.open_error = {13, category};
    require_failure(run(state), meios::text_read_failure_kind::open, meios::operation_kind::open, 13);
    REQUIRE(state.calls == std::vector<std::string>{"status", "open"});
    REQUIRE(state.closes == 0);
}
TEST_CASE("read refusal closes once and returns no partial bytes", "[text_reader]")
{
    script state     = regular;
    state.chunks     = {"partial"};
    state.read_error = {17, category};
    require_failure(run(state), meios::text_read_failure_kind::read, meios::operation_kind::read, 17);
    REQUIRE(state.calls == std::vector<std::string>{"status", "open", "read", "error", "close"});
    REQUIRE(state.closes == 1);
}
TEST_CASE("close refusal is observable and not retried", "[text_reader]")
{
    script state      = regular;
    state.close_error = {19, category};
    require_failure(run(state), meios::text_read_failure_kind::close, meios::operation_kind::close, 19);
    REQUIRE(state.calls == std::vector<std::string>{"status", "open", "read", "error", "close"});
    REQUIRE(state.closes == 1);
}
TEST_CASE("empty and embedded NUL content remain successful", "[text_reader]")
{
    script empty                               = regular;
    const meios::text_read_result empty_result = run(empty);
    REQUIRE(empty_result.has_value());
    REQUIRE(empty_result->empty());
    REQUIRE(empty.closes == 1);
    script binary                        = regular;
    binary.chunks                        = {std::string{"a\0b", 3}};
    const meios::text_read_result result = run(binary);
    REQUIRE(result.has_value());
    REQUIRE(*result == std::string{"a\0b", 3});
    REQUIRE(binary.closes == 1);
}
TEST_CASE("multi-chunk transfer uses the measured request and closes once", "[text_reader]")
{
    script state                         = regular;
    state.chunks                         = {"one", std::string{"t\0wo", 4}, "three"};
    const meios::text_read_result result = run(state);
    REQUIRE(result.has_value());
    REQUIRE(*result == std::string{"onet\0wothree", 12});
    REQUIRE(state.request == 65536);
    REQUIRE(state.closes == 1);
}
