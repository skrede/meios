cmake_minimum_required(VERSION 3.28)

project(meios_cmake_lifecycle NONE)

include(MeiosDeclareResource)

# Every name here acquires. A declaration pointed at a pre-placed tree returns before a claim is
# recorded, so a prune case built on one would find nothing to collect and pass without asserting
# anything.
#
# ',' separates the names for the same reason it separates the module fixture's assertions: ';' is
# already what separates the arguments the driver hands this sub-configure.
string(REPLACE "," ";" _names "${FX_RESOURCES}")
foreach(_name IN LISTS _names)
    meios_declare_resource(NAME ${_name} URL "${FX_URL}" HASH "${FX_HASH}")
endforeach()
