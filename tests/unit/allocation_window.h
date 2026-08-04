#ifndef HPP_GUARD_MEIOS_TEST_ALLOCATION_WINDOW_H
#define HPP_GUARD_MEIOS_TEST_ALLOCATION_WINDOW_H

#include <new>
#include <cstdlib>
#include <cstddef>

// Replaces the global allocation functions, so exactly one translation unit per test binary may
// include this header. Each stem that uses it is a single-source executable, which is what keeps
// that true. A failing allocation is the only portable way to reach a nonthrowing verb's handler,
// and the standard library allocates everywhere else, so the arming window must open and close
// around exactly one call.

namespace scratch_test
{

// [new.delete.single] requires a replacement operator new to return storage or throw bad_alloc, so
// the foreign failure models a consumer allocator that is non-conforming rather than merely unlucky.
enum class window_failure
{
    none,
    exhaustion,
    foreign
};

struct foreign_failure
{
};

inline bool window_open              = false;
inline window_failure window_failing = window_failure::none;
inline std::size_t window_uses       = 0;

inline void fail_when_armed()
{
    if(window_failing == window_failure::foreign)
        throw foreign_failure{};
    if(window_failing == window_failure::exhaustion)
        throw std::bad_alloc();
}

class allocation_window
{
public:
    explicit allocation_window(window_failure failure)
    {
        window_uses    = 0;
        window_failing = failure;
        window_open    = true;
    }

    ~allocation_window() { window_open = false; }

    allocation_window(const allocation_window &)            = delete;
    allocation_window &operator=(const allocation_window &) = delete;
    allocation_window(allocation_window &&)                 = delete;
    allocation_window &operator=(allocation_window &&)      = delete;
};

}

void *operator new(std::size_t size)
{
    if(scratch_test::window_open)
    {
        ++scratch_test::window_uses;
        scratch_test::fail_when_armed();
    }
    void *block = std::malloc(size != 0 ? size : 1);
    if(block == nullptr)
        throw std::bad_alloc();
    return block;
}

void *operator new[](std::size_t size)
{
    return operator new(size);
}

void operator delete(void *block) noexcept
{
    std::free(block);
}

void operator delete[](void *block) noexcept
{
    std::free(block);
}

void operator delete(void *block, std::size_t) noexcept
{
    std::free(block);
}

void operator delete[](void *block, std::size_t) noexcept
{
    std::free(block);
}

#endif
