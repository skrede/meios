#ifndef HPP_GUARD_MEIOS_UNIT_STACK_METER_H
#define HPP_GUARD_MEIOS_UNIT_STACK_METER_H

#if defined(_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <unistd.h>
    #include <pthread.h>
    #include <sys/mman.h>
#endif

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <functional>

namespace stack_meter
{

// The stack figures belong to an unoptimized, uninstrumented build, since optimization and
// sanitizer instrumentation each change the size of every frame.
#if defined(_WIN32) || defined(__OPTIMIZE__) || defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
inline constexpr bool headroom_measured = false;
#elif defined(__has_feature)
    #if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
inline constexpr bool headroom_measured = false;
    #else
inline constexpr bool headroom_measured = true;
    #endif
#else
inline constexpr bool headroom_measured = true;
#endif

#if defined(_WIN32)

namespace detail
{

inline DWORD WINAPI trampoline(LPVOID work)
{
    (*static_cast<const std::function<void()> *>(work))();
    return 0;
}

}

inline bool run_on_stack(std::size_t bytes, const std::function<void()> &work)
{
    const LPVOID handed = const_cast<std::function<void()> *>(&work);
    const HANDLE thread = CreateThread(nullptr, bytes, detail::trampoline, handed, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    if(thread == nullptr)
        return false;
    const bool joined = WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0;
    CloseHandle(thread);
    return joined;
}

inline std::optional<std::size_t> stack_used(std::size_t, const std::function<void()> &)
{
    return std::nullopt;
}

#else

namespace detail
{

inline constexpr std::uint8_t fill = 0xa5;

inline void *trampoline(void *work)
{
    (*static_cast<const std::function<void()> *>(work))();
    return nullptr;
}

// A region whose lowest page faults on any access, so a thread that runs off the end of the stack
// above it stops there instead of writing into whatever the mapping borders.
class guarded_stack
{
public:
    explicit guarded_stack(std::size_t bytes)
            : m_page(static_cast<std::size_t>(sysconf(_SC_PAGESIZE)))
            , m_size((bytes + m_page - 1) / m_page * m_page)
            , m_base(mmap(nullptr, m_page + m_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0))
    {
        if(m_base != MAP_FAILED && mprotect(m_base, m_page, PROT_NONE) != 0)
        {
            munmap(m_base, m_page + m_size);
            m_base = MAP_FAILED;
        }
        if(m_base != MAP_FAILED)
            std::memset(stack(), fill, m_size);
    }

    ~guarded_stack()
    {
        if(m_base != MAP_FAILED)
            munmap(m_base, m_page + m_size);
    }

    guarded_stack(const guarded_stack &)            = delete;
    guarded_stack &operator=(const guarded_stack &) = delete;

    bool mapped() const
    {
        return m_base != MAP_FAILED;
    }

    std::size_t size() const
    {
        return m_size;
    }

    std::uint8_t *stack() const
    {
        return static_cast<std::uint8_t *>(m_base) + m_page;
    }

    std::size_t touched() const
    {
        std::size_t untouched = 0;
        while(untouched < m_size && stack()[untouched] == fill)
            ++untouched;
        return m_size - untouched;
    }

private:
    std::size_t m_page;
    std::size_t m_size;
    void *m_base;
};

inline bool run_on(const guarded_stack &region, const std::function<void()> &work)
{
    pthread_attr_t attributes;
    if(!region.mapped() || pthread_attr_init(&attributes) != 0)
        return false;
    pthread_t thread;
    void *handed       = const_cast<std::function<void()> *>(&work);
    const bool started = pthread_attr_setstack(&attributes, region.stack(), region.size()) == 0 && pthread_create(&thread, &attributes, trampoline, handed) == 0;
    pthread_attr_destroy(&attributes);
    return started && pthread_join(thread, nullptr) == 0;
}

}

inline bool run_on_stack(std::size_t bytes, const std::function<void()> &work)
{
    const detail::guarded_stack region(bytes);
    return detail::run_on(region, work);
}

inline std::optional<std::size_t> stack_used(std::size_t bytes, const std::function<void()> &work)
{
    const detail::guarded_stack region(bytes);
    if(!detail::run_on(region, work))
        return std::nullopt;
    return region.touched();
}

#endif

// A shape keeps ninefold headroom when nine times the stack its admitted chain adds still fits
// beside the fixed cost the same document pays with no level at all.
inline bool keeps_ninefold(std::size_t base, std::size_t loaded, std::size_t thread)
{
    return loaded <= base || base + 9 * (loaded - base) < thread;
}

}

#endif
