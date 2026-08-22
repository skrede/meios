#include <cstring>
#include <iostream>

// Stands in for the real binary only as far as the build rule can see it: the rule invokes the
// flatten verb and publishes what arrives on standard output, so printing a document nothing else
// in the tree carries is enough to show which binary the rule actually ran.
int main(int argc, char **argv)
{
    if(argc < 2 || std::strcmp(argv[1], "flatten") != 0)
    {
        std::cerr << "stub: expected the flatten verb\n";
        return 2;
    }
    std::cout << "<robot name=\"harness_stub_cli\"/>\n";
    return 0;
}
