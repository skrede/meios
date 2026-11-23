#ifndef HPP_GUARD_MEIOS_UNIT_CLI_VERBS_FIXTURE_H
#define HPP_GUARD_MEIOS_UNIT_CLI_VERBS_FIXTURE_H

#include <string>
#include <cstddef>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace
{

inline std::string fixture(const std::string &name)
{
    return (std::filesystem::path(MEIOS_URDF_FIXTURE_DIR) / name).string();
}

// Redirects std::cout for the scope of one verb call so a test can assert on the
// bytes a verb writes, restoring the stream on destruction.
class cout_capture
{
public:
    cout_capture()
            : m_buffer()
            , m_saved(std::cout.rdbuf(m_buffer.rdbuf()))
    {
    }

    ~cout_capture()
    {
        std::cout.rdbuf(m_saved);
    }

    cout_capture(const cout_capture &)            = delete;
    cout_capture &operator=(const cout_capture &) = delete;

    std::string str() const
    {
        return m_buffer.str();
    }

private:
    std::ostringstream m_buffer;
    std::streambuf *m_saved;
};

// The twin of cout_capture for stderr: verbs relay typed diagnostics through a
// log_sink_s bound to std::cerr, so a test swaps std::cerr's rdbuf to assert on the
// bytes a failing verb writes.
class cerr_capture
{
public:
    cerr_capture()
            : m_buffer()
            , m_saved(std::cerr.rdbuf(m_buffer.rdbuf()))
    {
    }

    ~cerr_capture()
    {
        std::cerr.rdbuf(m_saved);
    }

    cerr_capture(const cerr_capture &)            = delete;
    cerr_capture &operator=(const cerr_capture &) = delete;

    std::string str() const
    {
        return m_buffer.str();
    }

private:
    std::ostringstream m_buffer;
    std::streambuf *m_saved;
};

inline std::size_t occurrences(const std::string &haystack, const std::string &needle)
{
    std::size_t count = 0;
    for(std::size_t pos = haystack.find(needle); pos != std::string::npos; pos = haystack.find(needle, pos + needle.size()))
        ++count;
    return count;
}

}

#endif
