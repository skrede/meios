#include <meios/version.h>
#include <meios/detail/xml_engine.h>

#include <iostream>

int main()
{
    if (!meios::detail::xml_engine_ready())
    {
        std::cerr << "meios integration test FAILED (pugixml link anchor unresolved)\n";
        return 1;
    }
    std::cout << "meios integration test PASSED (version = " << meios::version() << ")\n";
    return 0;
}
