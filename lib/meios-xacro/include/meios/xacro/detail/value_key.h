#ifndef HPP_GUARD_MEIOS_XACRO_VALUE_KEY_H
#define HPP_GUARD_MEIOS_XACRO_VALUE_KEY_H

#include "meios/xacro/value_kind.h"

#include <cmath>
#include <string>
#include <cstdint>
#include <variant>
#include <optional>
#include <string_view>

namespace meios
{

namespace detail
{

// Python compares an int against a float at arbitrary precision, which a bare int64_t == double
// does not: the integer converts to double and loses precision above 2^53, silently folding two
// distinct large keys into one. Truncating the double back is exact over the whole int64 range.
inline bool integer_equals_real(std::int64_t number, double other)
{
    constexpr double span = 9223372036854775808.0;
    if(!(other >= -span) || !(other < span))
        return false;
    return other == std::trunc(other) && static_cast<std::int64_t>(other) == number;
}

// The five kinds a document can write in key position, declared in value_kind's order so the
// held alternative's index is the kind, the way the value variant reads. A sequence or a
// mapping in key position is unrepresentable here rather than refused after the fact.
//
// Equality is Python's equality on the key object: the numeric tower — boolean, integer and
// real — compares by numeric value, and text and null sit outside that tower, so they compare
// only within their own kind. A document writing 1 and true writes one key; one writing '1'
// and 1 writes two.
class scalar_key
{
public:
    scalar_key() : m_data() {}
    explicit scalar_key(bool flag) : m_data(flag) {}
    explicit scalar_key(double number) : m_data(number) {}
    explicit scalar_key(std::int64_t number) : m_data(number) {}
    explicit scalar_key(std::string text) : m_data(std::move(text)) {}

    // The value type answers the same hazard by deleting this overload, which a key cannot do:
    // a key written as a literal is the ordinary case here, and without the overload the
    // pointer would convert to bool ahead of std::string and become a boolean key.
    explicit scalar_key(const char *text) : m_data(std::string(text)) {}

    value_kind kind() const { return static_cast<value_kind>(m_data.index()); }

    std::optional<bool> boolean() const { return held<bool>(); }
    std::optional<double> real() const { return held<double>(); }
    std::optional<std::int64_t> integer() const { return held<std::int64_t>(); }
    std::optional<std::string> text() const { return held<std::string>(); }

    bool operator==(const scalar_key &other) const
    {
        if(!in_numeric_tower() || !other.in_numeric_tower())
            return m_data == other.m_data;
        return same_number(other);
    }

    bool operator==(std::string_view other) const
    {
        const std::string *stored = std::get_if<std::string>(&m_data);
        return stored != nullptr && *stored == other;
    }

private:
    std::variant<std::monostate, bool, std::int64_t, double, std::string> m_data;

    template <typename T>
    std::optional<T> held() const
    {
        const T *stored = std::get_if<T>(&m_data);
        return stored == nullptr ? std::nullopt : std::optional<T>(*stored);
    }

    bool in_numeric_tower() const
    {
        return kind() == value_kind::boolean || kind() == value_kind::integer
            || kind() == value_kind::real;
    }

    std::optional<std::int64_t> whole() const
    {
        if(const bool *flag = std::get_if<bool>(&m_data))
            return static_cast<std::int64_t>(*flag);
        return integer();
    }

    bool same_number(const scalar_key &other) const
    {
        const std::optional<std::int64_t> mine = whole();
        const std::optional<std::int64_t> theirs = other.whole();
        if(mine && theirs)
            return *mine == *theirs;
        if(mine)
            return integer_equals_real(*mine, *other.real());
        if(theirs)
            return integer_equals_real(*theirs, *real());
        return *real() == *other.real();
    }
};

}

}

#endif
