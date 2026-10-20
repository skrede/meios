#ifndef HPP_GUARD_MEIOS_XACRO_INCLUDE_OPTIONS_H
#define HPP_GUARD_MEIOS_XACRO_INCLUDE_OPTIONS_H

namespace meios
{

// climb_above_document lifts a relative include's boundary; follow_outside_links lifts the one an include,
// an auxiliary document and an asset beside its document keep, and a followed link's answer is final.
// Each reads where the operating system resolves, so a description can name any file the process can open,
// except that a climb in a document a layer serves without a path stays in its package; only the caller sets them.
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
