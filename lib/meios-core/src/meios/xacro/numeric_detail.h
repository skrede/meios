#ifndef HPP_GUARD_MEIOS_XACRO_NUMERIC_DETAIL_H
#define HPP_GUARD_MEIOS_XACRO_NUMERIC_DETAIL_H

#include <string>
#include <version>

#if defined(__cpp_lib_to_chars) && __cpp_lib_to_chars >= 201611L
    #define MEIOS_HAS_FLOAT_CHARCONV 1
#else
    #define MEIOS_HAS_FLOAT_CHARCONV 0
    #if defined(_WIN32)
        #include <clocale>
    #else
        #include <locale.h>
        #if defined(__APPLE__)
            // Under a strict -std (CMAKE_CXX_EXTENSIONS OFF -> __STRICT_ANSI__) some
            // macOS SDKs hide the POSIX xlocale extensions (locale_t, newlocale,
            // uselocale, LC_ALL_MASK) from <locale.h>; <xlocale.h> declares them
            // unconditionally. Harmless where <locale.h> already exposes them.
            #include <xlocale.h>
        #endif
    #endif
#endif

namespace meios::detail
{

#if !MEIOS_HAS_FLOAT_CHARCONV

double strtod_c_locale(const char *first, char **last);

#endif

std::string print_double_shortest(double number);

}

#endif
