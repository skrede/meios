#ifndef HPP_GUARD_MEIOS_CORE_IO_TEXT_READER_NT_H
#define HPP_GUARD_MEIOS_CORE_IO_TEXT_READER_NT_H

#include <windows.h>
#include <winternl.h>

#include <limits>
#include <string>
#include <vector>
#include <cstdint>
#include <utility>
#include <optional>

namespace meios::detail::native
{

using nt_create_file_fn  = NTSTATUS(NTAPI *)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK, PLARGE_INTEGER, ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);
using rtl_status_fn      = ULONG(NTAPI *)(NTSTATUS);
using rtl_dos_path_fn    = NTSTATUS(NTAPI *)(PCWSTR, PUNICODE_STRING, PWSTR *, PVOID);
using rtl_free_string_fn = VOID(NTAPI *)(PUNICODE_STRING);

struct ntdll_api
{
    nt_create_file_fn create;
    rtl_status_fn status;
    rtl_dos_path_fn dos_path;
    rtl_free_string_fn free_string;
};

inline std::error_code win32_error(DWORD fallback) noexcept
{
    const DWORD value = GetLastError();
    return {static_cast<int>(value == ERROR_SUCCESS ? fallback : value), std::system_category()};
}

template<typename Function>
Function symbol(HMODULE module, const char *name) noexcept
{
    SetLastError(ERROR_SUCCESS);
    return reinterpret_cast<Function>(GetProcAddress(module, name));
}

inline expected<ntdll_api, std::error_code> load_api() noexcept
{
    HMODULE module = GetModuleHandleW(L"ntdll.dll");
    if(module == nullptr)
        return unexpected<std::error_code>(win32_error(ERROR_MOD_NOT_FOUND));
    const nt_create_file_fn create = symbol<nt_create_file_fn>(module, "NtCreateFile");
    if(create == nullptr)
        return unexpected<std::error_code>(win32_error(ERROR_PROC_NOT_FOUND));
    const rtl_status_fn status = symbol<rtl_status_fn>(module, "RtlNtStatusToDosError");
    if(status == nullptr)
        return unexpected<std::error_code>(win32_error(ERROR_PROC_NOT_FOUND));
    const rtl_dos_path_fn dos_path = symbol<rtl_dos_path_fn>(module, "RtlDosPathNameToNtPathName_U_WithStatus");
    if(dos_path == nullptr)
        return unexpected<std::error_code>(win32_error(ERROR_PROC_NOT_FOUND));
    const rtl_free_string_fn free_string = symbol<rtl_free_string_fn>(module, "RtlFreeUnicodeString");
    if(free_string == nullptr)
        return unexpected<std::error_code>(win32_error(ERROR_PROC_NOT_FOUND));
    return ntdll_api{create, status, dos_path, free_string};
}

class handle_owner
{
public:
    explicit handle_owner(HANDLE handle) noexcept
            : m_handle(handle)
    {
    }
    handle_owner(handle_owner &&other) noexcept
            : m_handle(std::exchange(other.m_handle, nullptr))
    {
    }
    handle_owner &operator=(handle_owner &&other) noexcept
    {
        if(m_handle != nullptr)
            (void)CloseHandle(m_handle);
        m_handle = std::exchange(other.m_handle, nullptr);
        return *this;
    }
    handle_owner(const handle_owner &)            = delete;
    handle_owner &operator=(const handle_owner &) = delete;
    ~handle_owner()
    {
        if(m_handle != nullptr)
            (void)CloseHandle(m_handle);
    }

    HANDLE get() const noexcept
    {
        return m_handle;
    }

    HANDLE release() noexcept
    {
        return std::exchange(m_handle, nullptr);
    }

private:
    HANDLE m_handle;
};

class unicode_owner
{
public:
    unicode_owner(UNICODE_STRING value, rtl_free_string_fn release) noexcept
            : m_value(value)
            , m_release(release)
    {
    }
    unicode_owner(const unicode_owner &)            = delete;
    unicode_owner &operator=(const unicode_owner &) = delete;
    ~unicode_owner()
    {
        m_release(&m_value);
    }

private:
    UNICODE_STRING m_value;
    rtl_free_string_fn m_release;
};

using handle_result         = expected<handle_owner, std::error_code>;
using checked_handle_result = expected<handle_owner, text_read_failure>;

inline handle_result nt_open(const ntdll_api &api, HANDLE root, PUNICODE_STRING name, ACCESS_MASK access, ULONG options) noexcept
{
    OBJECT_ATTRIBUTES attributes;
    InitializeObjectAttributes(&attributes, name, OBJ_CASE_INSENSITIVE, root, nullptr);
    IO_STATUS_BLOCK block{};
    HANDLE handle         = nullptr;
    const NTSTATUS result = api.create(&handle, access, &attributes, &block, nullptr, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN,
                                       options | FILE_OPEN_REPARSE_POINT | FILE_SYNCHRONOUS_IO_NONALERT, nullptr, 0);
    if(result < 0)
        return unexpected<std::error_code>({static_cast<int>(api.status(result)), std::system_category()});
    return handle_owner{handle};
}

inline std::optional<text_read_failure> validate(HANDLE handle, bool final) noexcept
{
    FILE_ATTRIBUTE_TAG_INFO attributes{};
    if(!GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &attributes, sizeof(attributes)))
        return native_failure(text_read_failure_kind::status, operation_kind::status, win32_error(ERROR_GEN_FAILURE));
    if((attributes.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
        return native_failure(text_read_failure_kind::open, operation_kind::open, {static_cast<int>(ERROR_CANT_ACCESS_FILE), std::system_category()});
    if(!final)
        return std::nullopt;
    if((attributes.FileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        return native_failure(text_read_failure_kind::non_regular, operation_kind::status, {});
    SetLastError(ERROR_SUCCESS);
    const DWORD type = GetFileType(handle);
    if(type == FILE_TYPE_UNKNOWN && GetLastError() != ERROR_SUCCESS)
        return native_failure(text_read_failure_kind::status, operation_kind::status, win32_error(ERROR_GEN_FAILURE));
    if(type != FILE_TYPE_DISK)
        return native_failure(text_read_failure_kind::non_regular, operation_kind::status, {});
    return std::nullopt;
}

inline expected<UNICODE_STRING, std::error_code> component_name(std::wstring &component) noexcept
{
    const std::size_t bytes = component.size() * sizeof(wchar_t);
    if(bytes > (std::numeric_limits<std::uint16_t>::max)())
        return unexpected<std::error_code>({static_cast<int>(ERROR_FILENAME_EXCED_RANGE), std::system_category()});
    const std::uint16_t length = static_cast<std::uint16_t>(bytes);
    return UNICODE_STRING{length, length, const_cast<PWSTR>(component.c_str())};
}

inline std::vector<std::filesystem::path> components(const std::filesystem::path &relative)
{
    std::vector<std::filesystem::path> result;
    for(const std::filesystem::path &component : relative)
        if(component != ".")
            result.push_back(component);
    return result;
}

inline bool valid_request(const std::filesystem::path &relative)
{
    if(relative.empty() || relative.is_absolute() || relative.has_root_path())
        return false;
    bool has_component = false;
    for(const std::filesystem::path &component : relative)
    {
        if(component == "..")
            return false;
        has_component = has_component || component != ".";
    }
    return has_component;
}

}

#endif
