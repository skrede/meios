#include <meios/urdf.h>
#include <meios/model.h>

int main()
{
    const meios::expected<meios::load_result, meios::load_error> loaded =
        meios::load("meios-consumer-has-no-document.urdf");
    static_cast<void>(loaded);
    return 0;
}
