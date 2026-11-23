#ifndef HPP_GUARD_MEIOS_EVAL_KEPT_STREAM_MODES_H
#define HPP_GUARD_MEIOS_EVAL_KEPT_STREAM_MODES_H

// Only a build against the release DLL C runtime (/MD), the one the release interpreter uses, keeps the modes. A MinGW
// build or one running a debug Python keeps none, although either can share its descriptors with the interpreter.
#if defined(_WIN32) && defined(_DLL) && !defined(_DEBUG)
    #define MEIOS_KEEPS_STREAM_MODES 1

    #include <io.h>
    #include <fcntl.h>

    #include <array>
    #include <cstdio>
    #include <cstdint>
    #include <cstdlib>
#else
    #define MEIOS_KEEPS_STREAM_MODES 0
#endif

namespace meios::detail
{

// The interpreter's file objects switch each standard descriptor they wrap to binary mode on Windows, so the
// modes are read before the start and set back after it.
class kept_stream_modes
{
#if MEIOS_KEEPS_STREAM_MODES
    struct kept_mode
    {
        std::int32_t descriptor;
        std::int32_t mode;
    };

    // A descriptor the program closed keeps its stream's number, and the runtime ends the process when such a
    // descriptor is set; Python checks its own standard descriptors with the handler silenced the same way.
    class silenced_parameter_checks
    {
    public:
        silenced_parameter_checks()
                : m_previous(_set_thread_local_invalid_parameter_handler(ignored))
        {
        }

        ~silenced_parameter_checks()
        {
            _set_thread_local_invalid_parameter_handler(m_previous);
        }

        silenced_parameter_checks(const silenced_parameter_checks &)            = delete;
        silenced_parameter_checks &operator=(const silenced_parameter_checks &) = delete;

    private:
        _invalid_parameter_handler m_previous;

        static void ignored(const wchar_t *, const wchar_t *, const wchar_t *, std::uint32_t, std::uintptr_t)
        {
        }
    };
#endif

public:
#if MEIOS_KEEPS_STREAM_MODES
    kept_stream_modes()
            : m_kept{kept(_fileno(stdin)), kept(_fileno(stdout)), kept(_fileno(stderr))}
    {
    }

    ~kept_stream_modes()
    {
        const silenced_parameter_checks silenced;
        // A Unicode translation mode is left binary, since Python writes bytes there and such a mode reads them as wide characters.
        for(const kept_mode &one : m_kept)
        {
            if(one.mode == _O_TEXT || one.mode == _O_BINARY)
                _setmode(one.descriptor, one.mode);
        }
    }
#else
    // Written out rather than defaulted, so the start's guard is not an unused variable, an error under -Werror.
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
#if MEIOS_KEEPS_STREAM_MODES
    std::array<kept_mode, 3> m_kept;

    static kept_mode kept(std::int32_t descriptor)
    {
        if(descriptor < 0)
            return kept_mode{descriptor, -1};
        const silenced_parameter_checks silenced;
        const std::int32_t mode = _setmode(descriptor, _O_BINARY);
        if(mode >= 0)
            _setmode(descriptor, mode);
        return kept_mode{descriptor, mode};
    }
#endif
};

}

#endif
