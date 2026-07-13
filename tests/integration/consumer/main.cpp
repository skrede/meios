#include <meios/version.h>

#include <iostream>

int main()
{
    std::cout << "meios integration test PASSED (version = " << meios::version() << ")\n";
    return 0;
}
