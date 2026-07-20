# Consumer guide: `load()` → `model`

The consumer tier is a single call. Hand `load()` a path to a URDF or xacro description; it reads the
file, expands xacro, resolves `package://` and `$(find)` references, builds a global material table,
and reconstructs the kinematic topology. What you get back is an
`expected<model<double>, load_error>`: on success a fully resolved `model`, on failure a typed
`file:line` diagnostic on the error channel. There is no intermediate serialization format and no
silent partial result — a description that cannot be resolved fails loudly.

## Loading a description and reading the model

The `model` carries the flattened `links`, `joints`, and `materials`, plus a reconstructed `topo`
whose `order` is the root-first visitation sequence and whose `parent_of` gives each link's parent by
index (a root reports `-1`). The program below loads a description, reports the diagnostic on failure,
and walks the topology on success.

<!-- meios:snippet name=load-and-read tu -->
```cpp
#include <meios/urdf.h>

#include <cstddef>
#include <iostream>

int main()
{
    const auto robot = meios::load("two_link.urdf");
    if (!robot)
    {
        const meios::load_error &err = robot.error();
        std::cout << "load failed at " << meios::to_string(err.loc) << ": " << err.message << '\n';
        return 1;
    }

    std::cout << "loaded " << robot->links.size() << " links\n";

    std::cout << "root-first order:";
    for (const int index : robot->topo.order)
        std::cout << ' ' << robot->links[static_cast<std::size_t>(index)].name;
    std::cout << '\n';

    for (std::size_t i = 0; i < robot->links.size(); ++i)
    {
        const int parent = robot->topo.parent_of[i];
        if (parent < 0)
            std::cout << robot->links[i].name << " is the root\n";
        else
            std::cout << robot->links[i].name << " descends from "
                      << robot->links[static_cast<std::size_t>(parent)].name << '\n';
    }
}
```

## The failure channel is not optional

`load()` returns `expected`, not a `model`, precisely so that a broken description cannot masquerade
as an empty-but-valid robot. Check the result before dereferencing it. On failure the `load_error`
carries a `loc` (the `file:line:column` where resolution gave up), a human-readable `message`, and a
typed `code` you can switch on instead of matching message text.

## A caveat consumers must know

`load(path).has_value() == true` does **not** mean the description resolved without complaint. The
convenience overload is silent by default: it surfaces the first fatal error through the `load_error`
channel and drops every `level::warn` diagnostic — an unresolved `package://` reference, a
warn-policy material collision, a topology issue under a `warn` policy. If you need to see those
warnings, inject a `log_sink` using the three-argument overload. The engine guide shows how to
capture diagnostics into your own record type; the same sink works for a pure consumer.
