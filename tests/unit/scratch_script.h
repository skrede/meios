#ifndef HPP_GUARD_MEIOS_TEST_SCRATCH_SCRIPT_H
#define HPP_GUARD_MEIOS_TEST_SCRATCH_SCRIPT_H

#include "meios/io/scratch_operations.h"

#include <string>
#include <vector>
#include <cstddef>
#include <filesystem>
#include <string_view>
#include <system_error>

namespace scratch_test
{

class error_category final : public std::error_category
{
public:
    const char *name() const noexcept override
    {
        return "scratch-test";
    }

    std::string message(int value) const override
    {
        return std::to_string(value);
    }
};

inline const error_category category;

struct scratch_script
{
    scratch_script()
            : cursor(0)
            , stem_error()
            , narrow_error()
            , next_write_error()
            , next_create_error()
            , next_publish_error()
            , stems()
            , calls()
    {
    }

    std::size_t cursor;
    std::error_code stem_error;
    std::error_code narrow_error;
    std::error_code next_write_error;
    std::error_code next_create_error;
    std::error_code next_publish_error;
    std::vector<std::string> stems;
    std::vector<std::string> calls;
};

// Only the verbs a case scripts are answered here; every other verb runs for real, so what a
// sweep measures is the production decision table rather than a model of it.
class scripted_scratch_operations final : public meios::detail::scratch_operations
{
public:
    explicit scripted_scratch_operations(scratch_script &script)
            : m_script(script)
    {
    }

    meios::detail::scratch_stem_result stem() const noexcept override
    {
        m_script.calls.emplace_back("stem");
        if(m_script.stem_error)
            return meios::unexpected<meios::operation_failure>({meios::operation_kind::create, m_script.stem_error});
        if(m_script.cursor == m_script.stems.size())
            return meios::detail::default_scratch_operations().stem();
        return m_script.stems[m_script.cursor++];
    }

    // The injected code replaces a create that really succeeded, and the directory that
    // create left behind is removed with it, so a case can place a real collision ahead of an
    // injected permanent failure and see them in that order.
    meios::detail::scratch_step_result create_exclusive(const std::filesystem::path &path) const noexcept override
    {
        m_script.calls.emplace_back("create");
        const meios::detail::scratch_step_result created = meios::detail::default_scratch_operations().create_exclusive(path);
        if(!created || !m_script.next_create_error)
            return created;
        const std::error_code error = m_script.next_create_error;
        m_script.next_create_error.clear();
        meios::detail::default_scratch_operations().remove_tree(path);
        return meios::unexpected<meios::operation_failure>({meios::operation_kind::create, error});
    }

    meios::detail::scratch_step_result narrow_to_owner(const std::filesystem::path &path) const noexcept override
    {
        m_script.calls.emplace_back("narrow");
        if(m_script.narrow_error)
            return meios::unexpected<meios::operation_failure>({meios::operation_kind::permissions, m_script.narrow_error});
        return meios::detail::default_scratch_operations().narrow_to_owner(path);
    }

    void remove_tree(const std::filesystem::path &path) const noexcept override
    {
        m_script.calls.emplace_back("remove");
        meios::detail::default_scratch_operations().remove_tree(path);
    }

    // Write and publish inject ahead of the real verb rather than behind it: a publication that
    // reported failure must not have moved the temporary onto the target, which is the very
    // property a rollback case exists to check.
    meios::detail::scratch_step_result write_bytes(const std::filesystem::path &path, std::string_view bytes) const noexcept override
    {
        m_script.calls.emplace_back("write");
        if(const std::error_code error = consume(m_script.next_write_error))
            return meios::unexpected<meios::operation_failure>({meios::operation_kind::write, error});
        return meios::detail::default_scratch_operations().write_bytes(path, bytes);
    }

    meios::detail::scratch_step_result publish(const std::filesystem::path &from, const std::filesystem::path &to) const noexcept override
    {
        m_script.calls.emplace_back("publish");
        if(const std::error_code error = consume(m_script.next_publish_error))
            return meios::unexpected<meios::operation_failure>({meios::operation_kind::publish, error});
        return meios::detail::default_scratch_operations().publish(from, to);
    }

private:
    scratch_script &m_script;

    static std::error_code consume(std::error_code &channel)
    {
        const std::error_code error = channel;
        channel.clear();
        return error;
    }
};

}

#endif
