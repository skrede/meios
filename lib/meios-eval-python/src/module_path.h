#ifndef HPP_GUARD_MEIOS_EVAL_MODULE_PATH_H
#define HPP_GUARD_MEIOS_EVAL_MODULE_PATH_H

#include <pybind11/embed.h>

#include <string>
#include <cstdlib>
#include <string_view>

namespace meios::detail
{

template<typename Char>
std::basic_string<Char> without_empty_elements(std::basic_string_view<Char> listed, Char separator)
{
    std::basic_string<Char> kept;
    std::size_t start = 0;
    while(start <= listed.size())
    {
        std::size_t end = listed.find(separator, start);
        if(end == std::basic_string_view<Char>::npos)
            end = listed.size();
        if(end > start && !kept.empty())
            kept.push_back(separator);
        kept.append(listed.substr(start, end - start));
        start = end + 1;
    }
    return kept;
}

// CPython reads an empty element as the working directory, which safe_path does not keep off the module path.
#if defined(_WIN32)
inline PyStatus set_module_path(PyConfig &config)
{
    const wchar_t *const listed = _wgetenv(L"PYTHONPATH");
    if(config.use_environment == 0 || listed == nullptr || *listed == L'\0')
        return PyStatus_Ok();
    return PyConfig_SetString(&config, &config.pythonpath_env, without_empty_elements<wchar_t>(listed, L';').c_str());
}
#else
inline PyStatus set_module_path(PyConfig &config)
{
    const char *const listed = std::getenv("PYTHONPATH");
    if(config.use_environment == 0 || listed == nullptr || *listed == '\0')
        return PyStatus_Ok();
    return PyConfig_SetBytesString(&config, &config.pythonpath_env, without_empty_elements<char>(listed, ':').c_str());
}
#endif

}

#endif
