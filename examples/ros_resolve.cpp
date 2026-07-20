#include <meios/ros/ros_package_source.h>

#include <string>
#include <vector>
#include <iostream>

int main()
{
    meios::log_sink log;
    meios::ros_package_source source = meios::ros_package_source::from_environment(log);

    const std::vector<std::string> packages = source.packages();
    std::cout << "ROS package discovery found " << packages.size()
              << " package(s) in the environment\n";
    for(const std::string &package : packages)
        std::cout << "  " << package << '\n';

    return 0;
}
