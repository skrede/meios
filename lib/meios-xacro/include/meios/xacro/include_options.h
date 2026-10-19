#ifndef HPP_GUARD_MEIOS_XACRO_INCLUDE_OPTIONS_H
#define HPP_GUARD_MEIOS_XACRO_INCLUDE_OPTIONS_H

namespace meios
{

// climb_above_document lifts a relative include's boundary; follow_outside_links lifts the one an
// include, an auxiliary document and an asset beside its document keep. Each reads what the operating
// system resolves with no bound, so a description can name any file the process can open; only the caller sets them.
struct include_options
{
    include_options()
        : climb_above_document(false)
        , follow_outside_links(false)
    {
    }

    bool climb_above_document;
    bool follow_outside_links;
};

}

#endif
