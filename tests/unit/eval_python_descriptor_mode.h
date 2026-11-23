#ifndef HPP_GUARD_MEIOS_UNIT_EVAL_PYTHON_DESCRIPTOR_MODE_H
#define HPP_GUARD_MEIOS_UNIT_EVAL_PYTHON_DESCRIPTOR_MODE_H

#if defined(_WIN32)

    #include <cstdint>

    #include <io.h>
    #include <fcntl.h>

namespace
{

// Setting a mode answers the one it replaces, so the mode is read by setting text and putting the answer back.
inline std::int32_t descriptor_mode(std::int32_t descriptor)
{
    if(descriptor < 0)
        return -1;
    const std::int32_t mode = _setmode(descriptor, _O_TEXT);
    if(mode >= 0)
        _setmode(descriptor, mode);
    return mode;
}

}

#endif

#endif
