# Consumer guide: `load()` → `model`

The consumer tier is a single call. Hand `load()` a path to a URDF or xacro description; it reads the
file, expands xacro through the native evaluator compiled into the library, resolves `package://` and
`$(find)` references, builds a global material table, and reconstructs the kinematic topology. The
call needs no interpreter and no capability you have to switch on — a default `load_options` already
carries the reader that answers an expression asking for an auxiliary configuration document. What
you get back is an
`expected<load_result, load_error>`: on success a `load_result` carrying the fully resolved `robot`,
every diagnostic the load raised, and the completeness claims the result is willing to make; on
failure a typed `file:line` diagnostic on the error channel, alongside that same diagnostic list.
There is no intermediate serialization format and no silent partial result — a description that
cannot be resolved fails loudly, and that includes one whose `package://` meshes are not there. The
[resources guide](resources-guide.md) covers how to opt into loading a description without its
assets, and what the result tells you when you do.

## Loading a description and reading the model

A successful load hands back a `load_result` whose `robot` is the model, alongside the full
`diagnostics` list and the completeness `claims` derived from it. The `model` carries the flattened
`links`, `joints`, and `materials`, plus a reconstructed `topo` whose `order` is the root-first
visitation sequence and whose `parent_of` gives each link's parent by index (a root reports `-1`).
The program below loads a description, reports the diagnostic on failure, and walks the topology on
success.

<!-- meios:snippet name=load-and-read tu -->
```cpp
#include <meios/urdf.h>

#include <cstddef>
#include <iostream>

int main()
{
    const auto loaded = meios::load("two_link.urdf");
    if (!loaded)
    {
        const meios::load_error &err = loaded.error();
        std::cout << "load failed at " << meios::to_string(err.loc) << ": " << err.message << '\n';
        return 1;
    }

    const meios::model<double> &robot = loaded->robot;

    std::cout << "loaded " << robot.links.size() << " links\n";

    std::cout << "root-first order:";
    for (const int index : robot.topo.order)
        std::cout << ' ' << robot.links[static_cast<std::size_t>(index)].name;
    std::cout << '\n';

    for (std::size_t i = 0; i < robot.links.size(); ++i)
    {
        const int parent = robot.topo.parent_of[i];
        if (parent < 0)
            std::cout << robot.links[i].name << " is the root\n";
        else
            std::cout << robot.links[i].name << " descends from "
                      << robot.links[static_cast<std::size_t>(parent)].name << '\n';
    }
}
```

## The failure channel is not optional

`load()` returns `expected`, not a `load_result`, precisely so that a broken description cannot
masquerade as an empty-but-valid robot. Check the result before dereferencing it. On failure the
`load_error` carries a `loc` (the `file:line:column` where resolution gave up), a human-readable
`message`, a typed `code` you can switch on instead of matching message text, and the same
`diagnostics` list the success arm carries — the primary error names where resolution gave up, the
list says everything else the document raised on the way there.

## A caveat consumers must know

`load(path).has_value() == true` does **not** mean the description resolved without complaint. The
convenience overload is silent by default: it surfaces the first fatal error through the `load_error`
channel and drops every `level::warn` diagnostic — an unresolved `package://` reference, a
`<visual>` naming a material nothing defines under a `warn` material policy, a topology issue under a
`warn` policy. (A material name *collision* is not one of these: it fails the load outright at every
setting.) If you need to see those
warnings, inject a `log_sink` using the three-argument overload. The engine guide shows how to
capture diagnostics into your own record type; the same sink works for a pure consumer.
