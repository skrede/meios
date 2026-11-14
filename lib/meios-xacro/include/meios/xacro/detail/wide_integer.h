#ifndef HPP_GUARD_MEIOS_XACRO_DETAIL_WIDE_INTEGER_H
#define HPP_GUARD_MEIOS_XACRO_DETAIL_WIDE_INTEGER_H

#include <string>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <charconv>
#include <optional>
#include <string_view>
#include <system_error>

namespace meios::detail
{

// CPython's default integer-to-text digit limit, sys.int_info.default_max_str_digits
// (docs.python.org/3/library/stdtypes.html#integer-string-conversion-length-limitation), so every
// integer a document can render fits and quadratic arithmetic on one stays cheap.
inline constexpr std::size_t wide_digit_ceiling = 4300;

// The canonical decimal spelling is the stored form because rendering it is what a document does
// with an integer outside the 64-bit range; arithmetic converts it to a binary magnitude where it
// computes.
class wide_integer
{
public:
    static std::optional<wide_integer> from_spelling(std::string_view text)
    {
        if(!canonical(text) || fits_64_bits(text))
            return std::nullopt;
        return wide_integer(std::string(text));
    }

    const std::string &spelling() const { return m_spelling; }

    bool negative() const { return m_spelling.front() == '-'; }

    bool operator==(const wide_integer &other) const = default;

private:
    std::string m_spelling;

    explicit wide_integer(std::string text) : m_spelling(std::move(text)) {}

    static bool canonical(std::string_view text)
    {
        const std::string_view digits = text.starts_with('-') ? text.substr(1) : text;
        return !digits.empty() && digits.size() <= wide_digit_ceiling && digits.front() != '0'
            && digits.find_first_not_of("0123456789") == std::string_view::npos;
    }

    static bool fits_64_bits(std::string_view text)
    {
        std::int64_t number = 0;
        const std::from_chars_result read = std::from_chars(text.data(), text.data() + text.size(), number);
        return read.ec == std::errc{};
    }
};

}

#endif
