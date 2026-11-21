#ifndef HPP_GUARD_MEIOS_EVAL_KEPT_INTERRUPT_H
#define HPP_GUARD_MEIOS_EVAL_KEPT_INTERRUPT_H

#include <csignal>

#if !defined(_WIN32)

    #include <signal.h>

#endif

namespace meios::detail
{

// CPython's signal module installs its interrupt handler when it is first imported over a default
// disposition, even with the interpreter's signal handlers off, so SIGINT's own is put back after.
class kept_interrupt
{
public:
#if defined(_WIN32)
    kept_interrupt()
            : m_kept(std::signal(SIGINT, SIG_DFL))
    {
        std::signal(SIGINT, m_kept);
    }

    ~kept_interrupt()
    {
        std::signal(SIGINT, m_kept);
    }
#else
    kept_interrupt()
            : m_kept(current())
    {
    }

    ~kept_interrupt()
    {
        sigaction(SIGINT, &m_kept, nullptr);
    }
#endif

    kept_interrupt(const kept_interrupt &)            = delete;
    kept_interrupt &operator=(const kept_interrupt &) = delete;

private:
#if defined(_WIN32)
    decltype(std::signal(SIGINT, SIG_DFL)) m_kept;
#else
    struct sigaction m_kept;

    static struct sigaction current()
    {
        struct sigaction found{};
        sigaction(SIGINT, nullptr, &found);
        return found;
    }
#endif
};

}

#endif
