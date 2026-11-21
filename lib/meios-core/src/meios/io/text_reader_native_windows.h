#ifndef HPP_GUARD_MEIOS_CORE_IO_TEXT_READER_NATIVE_WINDOWS_H
#define HPP_GUARD_MEIOS_CORE_IO_TEXT_READER_NATIVE_WINDOWS_H

#include "meios/io/text_reader_nt.h"
#include "meios/io/text_reader_nt_reparse.h"

#include <io.h>
#include <fcntl.h>

#include <cstdint>
#include <optional>

namespace meios::detail
{
namespace native
{

inline checked_handle_result settled_open(const ntdll_api &api, HANDLE root, PUNICODE_STRING name, ACCESS_MASK access, ULONG options, bool final) noexcept
{
    handle_result opened = nt_open(api, root, name, access, options, true);
    if(!opened)
        return unexpected<text_read_failure>(native_failure(text_read_failure_kind::open, operation_kind::open, opened.error()));
    const expected<FILE_ATTRIBUTE_TAG_INFO, text_read_failure> first = attributes_of(opened->get());
    if(!first)
        return unexpected<text_read_failure>(first.error());
    checked_handle_result settled = settle_reparse(api, root, name, access, options, std::move(*opened), *first);
    if(!settled)
        return settled;
    const expected<FILE_ATTRIBUTE_TAG_INFO, text_read_failure> attributes = attributes_of(settled->get());
    if(!attributes)
        return unexpected<text_read_failure>(attributes.error());
    if(const std::optional<text_read_failure> invalid = validate(*attributes, settled->get(), final))
        return unexpected<text_read_failure>(*invalid);
    return settled;
}

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
    return settled_open(api, nullptr, &name, FILE_LIST_DIRECTORY | FILE_TRAVERSE | FILE_READ_ATTRIBUTES | SYNCHRONIZE, FILE_DIRECTORY_FILE, false);
}

inline checked_handle_result open_component(const ntdll_api &api, HANDLE root, const std::filesystem::path &path, bool final) noexcept
{
    std::wstring component                         = path.native();
    expected<UNICODE_STRING, std::error_code> name = component_name(component);
    if(!name)
        return unexpected<text_read_failure>(native_failure(text_read_failure_kind::open, operation_kind::open, name.error()));
    const ACCESS_MASK access = (final ? FILE_READ_DATA : FILE_LIST_DIRECTORY | FILE_TRAVERSE) | FILE_READ_ATTRIBUTES | SYNCHRONIZE;
    // FILE_NON_DIRECTORY_FILE would turn a directory into a failed open; the last component is
    // opened unrestricted so its kind is named from the handle, as fstat does.
    return settled_open(api, root, &*name, access, final ? 0UL : FILE_DIRECTORY_FILE, final);
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

inline native_file_result stream_of(int descriptor) noexcept
{
    errno             = 0;
    std::FILE *stream = _fdopen(descriptor, "rb");
    if(stream != nullptr)
        return stream;
    const std::error_code error = native_crt_error();
    (void)_close(descriptor);
    return unexpected<text_read_failure>(native_failure(text_read_failure_kind::open, operation_kind::open, error));
}

inline std::optional<text_read_failure> regular_file_failure(int descriptor) noexcept
{
    struct _stat64 state{};
    errno = 0;
    if(_fstat64(descriptor, &state) != 0)
        return native_failure(text_read_failure_kind::status, operation_kind::status, native_crt_error());
    if((state.st_mode & _S_IFMT) != _S_IFREG)
        return native_failure(text_read_failure_kind::non_regular, operation_kind::status, {});
    return std::nullopt;
}

inline native_file_result stream_from(handle_owner handle) noexcept
{
    errno                = 0;
    const int descriptor = _open_osfhandle(reinterpret_cast<std::intptr_t>(handle.get()), _O_RDONLY | _O_BINARY);
    if(descriptor == -1)
        return unexpected<text_read_failure>(native_failure(text_read_failure_kind::open, operation_kind::open, native_crt_error()));
    (void)handle.release();
    return stream_of(descriptor);
}

}

inline native_file_result native_open(const std::filesystem::path &path) noexcept
{
    errno                = 0;
    const int descriptor = _wopen(path.c_str(), _O_RDONLY | _O_BINARY | _O_NOINHERIT);
    if(descriptor == -1)
        return unexpected<text_read_failure>(native_failure(text_read_failure_kind::open, operation_kind::open, native_crt_error()));
    if(const std::optional<text_read_failure> failure = native::regular_file_failure(descriptor))
    {
        (void)_close(descriptor);
        return unexpected<text_read_failure>(*failure);
    }
    return native::stream_of(descriptor);
}

inline native_file_result native_open_under(const std::filesystem::path &root, const std::filesystem::path &relative) noexcept
{
    if(!native::valid_request(relative))
        return unexpected<text_read_failure>(native_failure(text_read_failure_kind::uncontained, operation_kind::open, make_error_code(std::errc::invalid_argument)));
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
