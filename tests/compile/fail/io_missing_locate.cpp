#include <meios/io/source_handle.h>
#include <meios/io/package_source.h>

namespace
{

struct missing_locate
{
    meios::capability_descriptor capabilities() const
    {
        return { meios::source_kind::memory, false, false };
    }
};

}

int main()
{
    meios::source_handle handle{ missing_locate{} };
    (void)handle;
}
