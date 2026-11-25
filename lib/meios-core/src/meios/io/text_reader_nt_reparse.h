#ifndef HPP_GUARD_MEIOS_CORE_IO_TEXT_READER_NT_REPARSE_H
#define HPP_GUARD_MEIOS_CORE_IO_TEXT_READER_NT_REPARSE_H

#include "meios/io/text_reader_nt.h"

#include <windows.h>

#include <utility>
#include <algorithm>
#include <system_error>

namespace meios::detail::native
{

inline bool names_another_location(const FILE_ATTRIBUTE_TAG_INFO &attributes) noexcept
{
    return (attributes.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 && IsReparseTagNameSurrogate(attributes.ReparseTag) != 0;
}

inline bool same_index(HANDLE first, HANDLE second) noexcept
{
    BY_HANDLE_FILE_INFORMATION one{};
    BY_HANDLE_FILE_INFORMATION other{};
    if(!GetFileInformationByHandle(first, &one) || !GetFileInformationByHandle(second, &other))
        return false;
    return one.dwVolumeSerialNumber == other.dwVolumeSerialNumber && one.nFileIndexHigh == other.nFileIndexHigh && one.nFileIndexLow == other.nFileIndexLow;
}

// ReFS names a file by 128 bits, which the 64-bit index does not cover, so the index serves only
// where the file system reports no 128-bit identity.
inline bool same_file(HANDLE first, HANDLE second) noexcept
{
    FILE_ID_INFO one{};
    FILE_ID_INFO other{};
    if(!GetFileInformationByHandleEx(first, FileIdInfo, &one, sizeof(one)) || !GetFileInformationByHandleEx(second, FileIdInfo, &other, sizeof(other)))
        return same_index(first, second);
    return one.VolumeSerialNumber == other.VolumeSerialNumber && std::ranges::equal(one.FileId.Identifier, other.FileId.Identifier);
}

inline text_read_failure leaves_root() noexcept
{
    return native_failure(text_read_failure_kind::uncontained, operation_kind::open, {static_cast<int>(ERROR_CANT_ACCESS_FILE), std::system_category()});
}

// A handle opened on the reparse point itself does not pass through the filter that owns the tag,
// so the content is read through a second open, checked to name the same file.
inline checked_handle_result settle_reparse(const ntdll_api &api, HANDLE root, PUNICODE_STRING name, ACCESS_MASK access, ULONG options, handle_owner first,
                                            const FILE_ATTRIBUTE_TAG_INFO &attributes) noexcept
{
    if((attributes.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
        return first;
    if(names_another_location(attributes))
        return unexpected<text_read_failure>(leaves_root());
    handle_result second = nt_open(api, root, name, access, options, false);
    if(!second)
        return unexpected<text_read_failure>(native_failure(text_read_failure_kind::open, operation_kind::open, second.error()));
    if(!same_file(first.get(), second->get()))
        return unexpected<text_read_failure>(leaves_root());
    return std::move(*second);
}

}

#endif
