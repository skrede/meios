#ifndef HPP_GUARD_MEIOS_TEST_YAML_REPLACEMENT_OPERATIONS_H
#define HPP_GUARD_MEIOS_TEST_YAML_REPLACEMENT_OPERATIONS_H

#include "meios/io/text_reader_operations.h"

#include <utility>
#include <filesystem>
#include <system_error>

namespace acquisition_test
{

class replacement_operations final : public meios::detail::text_reader_operations
{
public:
    explicit replacement_operations(std::filesystem::path target)
            : m_target(std::move(target))
            , m_replaced(false)
    {
    }

    meios::detail::text_status_result status(const std::filesystem::path &path) const noexcept override
    {
        return meios::detail::default_text_reader_operations().status(path);
    }

    meios::detail::text_file_result open(const std::filesystem::path &path) const noexcept override
    {
        return meios::detail::default_text_reader_operations().open(path);
    }

    meios::detail::text_open_result open_under(const std::filesystem::path &root, const std::filesystem::path &relative) const noexcept override
    {
        std::error_code error;
        const std::filesystem::path path = root / relative;
        std::filesystem::remove(path, error);
        if(!error)
            std::filesystem::create_symlink(m_target, path, error);
        if(error)
            return meios::unexpected<meios::text_read_failure>({meios::text_read_failure_kind::open, {meios::operation_kind::open, error}});
        m_replaced = true;
        return meios::detail::default_text_reader_operations().open_under(root, relative);
    }

    bool replacement_created() const noexcept
    {
        return m_replaced;
    }

private:
    std::filesystem::path m_target;
    mutable bool m_replaced;
};

}

#endif
