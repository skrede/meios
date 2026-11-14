#include "eval_parser.h"
#include "eval_session.h"
#include "eval_numeric_ops.h"

#include "meios/xacro/value.h"

#include <cmath>
#include <string>
#include <utility>
#include <optional>

namespace meios::detail
{

// Length is the byte count of the produced text, not its code points: what the ceiling bounds is
// the memory the string occupies. The cumulative byte axis is charged the same production, so a
// load that never builds one oversized string is still bounded in what it builds altogether.
value text_result(parser &p, std::string text)
{
    const std::size_t length = text.size();
    if(!p.session.admits_string_length(length, p.log, p.anchor)
       || !p.session.charge_bytes(length, p.log, p.anchor))
    {
        p.refuse_exhausted();
        return value{};
    }
    return value{ std::move(text) };
}

bool admits_element(parser &p)
{
    if(p.session.charge_elements(1, p.log, p.anchor))
        return true;
    return p.refuse_exhausted();
}

// Only a real operand is bounded: an integer operand of a power computed as a real costs no more
// than the one std::pow call. An operand with no arithmetic meaning is admitted here and refuses
// below, where the diagnostic names its kind rather than a ceiling it never reached.
bool admits_power(parser &p, const value &a, const value &b)
{
    const std::optional<double> base = a.real();
    const std::optional<double> exponent = b.real();
    if((!base || p.session.admits_numeric_magnitude(std::fabs(*base), p.log, p.anchor))
       && (!exponent || p.session.admits_numeric_magnitude(std::fabs(*exponent), p.log, p.anchor)))
        return true;
    return p.refuse_exhausted();
}

}
