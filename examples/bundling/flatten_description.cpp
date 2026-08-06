#include "example_paths.h"

#include <meios/urdf.h>
#include <meios/bundle.h>

#include <string>
#include <sstream>
#include <iostream>

namespace
{

void report(const meios::emit_result &result, const std::string &document)
{
    std::cout << "flatten finished with status " << meios::to_string(result.status) << ", "
              << result.assets_seen << " asset(s) seen, " << result.unresolved << " unresolved\n";
    std::cout << "the flattened document is " << document.size() << " byte(s) opening with\n"
              << document.substr(0, 120) << '\n';
}

bool produced_a_urdf(const meios::emit_result &result, const std::string &document)
{
    if(result.status != meios::emit_status::ok)
        std::cout << "the flatten did not report success\n";
    else if(document.empty())
        std::cout << "the flatten reported success and produced nothing\n";
    else
        return true;
    return false;
}

}

int main()
{
    const meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load(examples::robot_description);
    if(!loaded)
    {
        std::cout << "load failed: " << loaded.error().message << '\n';
        return 1;
    }

    std::ostringstream stream;
    meios::log_sink_s log(std::cout);
    const meios::emit_result result = meios::flatten(loaded->robot, stream, log);

    const std::string document = stream.str();
    report(result, document);
    return produced_a_urdf(result, document) ? 0 : 1;
}
