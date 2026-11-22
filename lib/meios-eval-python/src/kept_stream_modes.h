#ifndef HPP_GUARD_MEIOS_EVAL_KEPT_STREAM_MODES_H
#define HPP_GUARD_MEIOS_EVAL_KEPT_STREAM_MODES_H

#include <array>
#include <cstdint>

#if defined(_WIN32)

    #include <io.h>
    #include <cstdio>
    #include <fcntl.h>

#endif

namespace meios::detail
{

// The interpreter's file objects switch each standard descriptor they wrap to binary mode on Windows, so the
// modes are read before the start and set back after it.
class kept_stream_modes
{
#if defined(_WIN32)
    struct kept_mode
    {
        std::int32_t descriptor;
        std::int32_t mode;
    };
#endif

public:
#if defined(_WIN32)
    // The descriptor table is the DLL C runtime's, shared with the interpreter only when both link it (/MD).
    kept_stream_modes()
            : m_kept{kept(_fileno(stdin)), kept(_fileno(stdout)), kept(_fileno(stderr))}
    {
    }

    ~kept_stream_modes()
    {
        for(const kept_mode &one : m_kept)
        {
            if(one.mode >= 0)
                _setmode(one.descriptor, one.mode);
        }
    }
#else
    kept_stream_modes()
    {
    }

    ~kept_stream_modes()
    {
    }
#endif

    kept_stream_modes(const kept_stream_modes &)            = delete;
    kept_stream_modes &operator=(const kept_stream_modes &) = delete;

private:
#if defined(_WIN32)
    std::array<kept_mode, 3> m_kept;

    static kept_mode kept(std::int32_t descriptor)
    {
        if(descriptor < 0)
            return kept_mode{descriptor, -1};
        const std::int32_t mode = _setmode(descriptor, _O_BINARY);
        if(mode >= 0)
            _setmode(descriptor, mode);
        return kept_mode{descriptor, mode};
    }
#endif
};

}

#endif
