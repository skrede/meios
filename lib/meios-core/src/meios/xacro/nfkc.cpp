#include "nfkc.h"
#include "unicode_data.h"

#include <span>
#include <string>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <optional>
#include <algorithm>
#include <functional>
#include <string_view>

namespace meios::detail
{

namespace
{

// The Unicode Standard, section 3.12 "Conjoining Jamo Behavior": Hangul syllables decompose into
// and compose from their jamo arithmetically.
constexpr char32_t s_base  = 0xAC00;
constexpr char32_t l_base  = 0x1100;
constexpr char32_t v_base  = 0x1161;
constexpr char32_t t_base  = 0x11A7;
constexpr char32_t l_count = 19;
constexpr char32_t v_count = 21;
constexpr char32_t t_count = 28;
constexpr char32_t n_count = v_count * t_count;
constexpr char32_t s_count = l_count * n_count;

bool syllable(char32_t point)
{
    return point >= s_base && point < s_base + s_count;
}

std::uint8_t combining_class(char32_t point)
{
    const std::span<const combining_run> runs = combining_runs();
    const auto found                          = std::ranges::lower_bound(runs, point, std::ranges::less{}, &combining_run::last);
    return found != runs.end() && found->first <= point ? found->combining_class : 0;
}

void decompose(char32_t point, std::u32string &out)
{
    const std::span<const decomposition> table = decompositions();
    const auto found                           = std::ranges::lower_bound(table, point, std::ranges::less{}, &decomposition::code_point);
    if(syllable(point))
    {
        const char32_t index = point - s_base;
        out += l_base + index / n_count;
        out += v_base + index % n_count / t_count;
        if(index % t_count != 0)
            out += t_base + index % t_count;
    }
    else if(found != table.end() && found->code_point == point)
    {
        const std::span<const char32_t> mapping = decomposition_pool().subspan(found->offset, found->length);
        out.append(mapping.begin(), mapping.end());
    }
    else
        out += point;
}

void reorder(std::u32string &points)
{
    const auto starter = [](char32_t point) { return combining_class(point) == 0; };
    for(auto run = points.begin(); run != points.end();)
    {
        run            = std::find_if_not(run, points.end(), starter);
        const auto end = std::find_if(run, points.end(), starter);
        // A stable sort keeps the marks of one class in their written order, in O(n log n) on a long run.
        std::ranges::stable_sort(run, end, std::ranges::less{}, combining_class);
        run = end;
    }
}

std::optional<char32_t> hangul_composite(char32_t starter, char32_t point)
{
    if(starter >= l_base && starter < l_base + l_count && point >= v_base && point < v_base + v_count)
        return s_base + ((starter - l_base) * v_count + (point - v_base)) * t_count;
    if(syllable(starter) && (starter - s_base) % t_count == 0 && point > t_base && point < t_base + t_count)
        return starter + (point - t_base);
    return std::nullopt;
}

std::optional<char32_t> composite(char32_t starter, char32_t point)
{
    if(const std::optional<char32_t> hangul = hangul_composite(starter, point))
        return hangul;
    const std::span<const composition> pairs = compositions();
    const auto key                           = [](const composition &one) { return std::pair(one.first, one.second); };
    const auto found                         = std::ranges::lower_bound(pairs, std::pair(starter, point), std::ranges::less{}, key);
    if(found != pairs.end() && found->first == starter && found->second == point)
        return found->composite;
    return std::nullopt;
}

// UAX #15 canonical composition (https://www.unicode.org/reports/tr15/): a point joins the last
// starter unless a point kept after that starter has a class of zero or one not lower than its own.
std::u32string composed(std::u32string_view points)
{
    std::u32string out;
    std::size_t starter = std::u32string::npos;
    std::uint8_t last   = 0;
    for(char32_t point : points)
    {
        const std::uint8_t point_class       = combining_class(point);
        const bool reachable                 = starter != std::u32string::npos && (last == 0 || last < point_class);
        const std::optional<char32_t> joined = reachable ? composite(out[starter], point) : std::nullopt;
        if(joined)
            out[starter] = *joined;
        else
        {
            starter = point_class == 0 ? out.size() : starter;
            last    = point_class;
            out += point;
        }
    }
    return out;
}

}

std::u32string nfkc(std::u32string_view points)
{
    std::u32string decomposed;
    for(char32_t point : points)
        decompose(point, decomposed);
    reorder(decomposed);
    return composed(decomposed);
}

}
