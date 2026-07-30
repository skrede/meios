#ifndef HPP_GUARD_MEIOS_CORE_IO_TEXT_READER_NATIVE_WINDOWS_H
#define HPP_GUARD_MEIOS_CORE_IO_TEXT_READER_NATIVE_WINDOWS_H

#include "meios/io/text_reader_nt.h"

#include <io.h>
#include <fcntl.h>

#include <cstdint>

namespace meios::detail
{
namespace native
{

inline checked_handle_result open_root(const ntdll_api &api, const std::filesystem::path &root) noexcept
{
    std::error_code error;
    const std::filesystem::path absolute = std::filesystem::absolute(root, error);
    if(error)
        return unexpected<text_read_failure>(native_failure(text_read_failure_kind::status, operation_kind::canonicalize, error));
    UNICODE_STRING name{};
    const NTSTATUS converted = api.dos_path(absolute.c_str(), &name, nullptr, nullptr);
    if(converted < 0)
        return unexpected<text_read_failure>(native_failure(text_read_failure_kind::open, operation_kind::open, {static_cast<int>(api.status(converted)), std::system_category()}));
    unicode_owner owned{name, api.free_string};
    handle_result opened = nt_open(api, nullptr, &name, FILE_LIST_DIRECTORY | FILE_TRAVERSE | FILE_READ_ATTRIBUTES | SYNCHRONIZE, FILE_DIRECTORY_FILE);
    if(!opened)
        return unexpected<text_read_failure>(native_failure(text_read_failure_kind::open, operation_kind::open, opened.error()));
    if(const std::optional<text_read_failure> invalid = validate(opened->get(), false))
        return unexpected<text_read_failure>(*invalid);
    return std::move(*opened);
}

inline checked_handle_result open_component(const ntdll_api &api, HANDLE root, const std::filesystem::path &path, bool final) noexcept
{
    std::wstring component                         = path.native();
    expected<UNICODE_STRING, std::error_code> name = component_name(component);
    if(!name)
        return unexpected<text_read_failure>(native_failure(text_read_failure_kind::open, operation_kind::open, name.error()));
    const ACCESS_MASK access = (final ? FILE_READ_DATA : FILE_LIST_DIRECTORY | FILE_TRAVERSE) | FILE_READ_ATTRIBUTES | SYNCHRONIZE;
    handle_result opened     = nt_open(api, root, &*name, access, final ? FILE_NON_DIRECTORY_FILE : FILE_DIRECTORY_FILE);
    if(!opened)
        return unexpected<text_read_failure>(native_failure(text_read_failure_kind::open, operation_kind::open, opened.error()));
    if(const std::optional<text_read_failure> invalid = validate(opened->get(), final))
        return unexpected<text_read_failure>(*invalid);
    return std::move(*opened);
}

inline checked_handle_result walk(const ntdll_api &api, handle_owner root, const std::vector<std::filesystem::path> &path) noexcept
{
    std::vector<handle_owner> handles;
    handles.reserve(path.size() + 1);
    handles.push_back(std::move(root));
    for(std::size_t index = 0; index < path.size(); ++index)
    {
        checked_handle_result opened = open_component(api, handles.back().get(), path[index], index + 1 == path.size());
        if(!opened)
            return unexpected<text_read_failure>(opened.error());
        handles.push_back(std::move(*opened));
    }
    return std::move(handles.back());
}

inline native_file_result stream_from(handle_owner handle) noexcept
{
    errno                = 0;
    const int descriptor = _open_osfhandle(reinterpret_cast<std::intptr_t>(handle.get()), _O_RDONLY | _O_BINARY);
    if(descriptor == -1)
        return unexpected<text_read_failure>(native_failure(text_read_failure_kind::open, operation_kind::open, native_crt_error()));
    (void)handle.release();
    errno             = 0;
    std::FILE *stream = _fdopen(descriptor, "rb");
    if(stream != nullptr)
        return stream;
    const std::error_code error = native_crt_error();
    (void)_close(descriptor);
    return unexpected<text_read_failure>(native_failure(text_read_failure_kind::open, operation_kind::open, error));
}

}

inline native_file_result native_open(const std::filesystem::path &path) noexcept
{
    errno                = 0;
    const int descriptor = _wopen(path.c_str(), _O_RDONLY | _O_BINARY | _O_NOINHERIT);
    if(descriptor == -1)
        return unexpected<text_read_failure>(native_failure(text_read_failure_kind::open, operation_kind::open, native_crt_error()));
    struct _stat64 state{};
    errno = 0;
    if(_fstat64(descriptor, &state) != 0)
    {
        const std::error_code error = native_crt_error();
        (void)_close(descriptor);
        return unexpected<text_read_failure>(native_failure(text_read_failure_kind::status, operation_kind::status, error));
    }
    if((state.st_mode & _S_IFMT) != _S_IFREG)
    {
        (void)_close(descriptor);
        return unexpected<text_read_failure>(native_failure(text_read_failure_kind::non_regular, operation_kind::status, {}));
    }
    errno             = 0;
    std::FILE *stream = _fdopen(descriptor, "rb");
    if(stream != nullptr)
        return stream;
    const std::error_code error = native_crt_error();
    (void)_close(descriptor);
    return unexpected<text_read_failure>(native_failure(text_read_failure_kind::open, operation_kind::open, error));
}

inline native_file_result native_open_under(const std::filesystem::path &root, const std::filesystem::path &relative) noexcept
{
    if(!native::valid_request(relative))
        return unexpected<text_read_failure>(native_failure(text_read_failure_kind::open, operation_kind::open, make_error_code(std::errc::invalid_argument)));
    expected<native::ntdll_api, std::error_code> api = native::load_api();
    if(!api)
        return unexpected<text_read_failure>(native_failure(text_read_failure_kind::open, operation_kind::open, api.error()));
    native::checked_handle_result opened_root = native::open_root(*api, root);
    if(!opened_root)
        return unexpected<text_read_failure>(opened_root.error());
    native::checked_handle_result final = native::walk(*api, std::move(*opened_root), native::components(relative));
    if(!final)
        return unexpected<text_read_failure>(final.error());
    return native::stream_from(std::move(*final));
}

}

#endif
