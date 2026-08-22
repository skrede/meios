#include <meios/xacro/substitution.h>

int main()
{
    meios::substitution resolved{};
    return resolved.ok ? 0 : 1;
}
