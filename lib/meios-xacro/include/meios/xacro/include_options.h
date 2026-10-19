#ifndef HPP_GUARD_MEIOS_XACRO_INCLUDE_OPTIONS_H
#define HPP_GUARD_MEIOS_XACRO_INCLUDE_OPTIONS_H

namespace meios
{

// Each member lifts a containment boundary a relative include keeps by default and reads what
// upstream xacro reads with no bound, so a description can then name any file the process can
// open; only the caller sets them.
struct include_options
{
    include_options()
        : climb_above_document(false)
    {
    }

    bool climb_above_document;
};

}

#endif
