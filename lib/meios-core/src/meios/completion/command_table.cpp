#include "meios/completion/command_table.h"

namespace meios
{

namespace
{

flag_spec package_path_flag()
{
    return flag_spec{ "--package-path",
                      "Additional package search root, repeatable.", flag_kind::value, false };
}

positional_spec model_positional()
{
    return positional_spec{ "model", "Path to the URDF or xacro document.", true };
}

command_spec flatten_command()
{
    return command_spec{ "flatten", "flatten",
                         "Expand xacro and resolve a URDF, printing the flattened document.",
                         { package_path_flag() }, { model_positional() } };
}

command_spec bundle_command()
{
    return command_spec{ "bundle", "bundle",
                         "Write a self-contained package folder with rewritten references.",
                         { package_path_flag(),
                           { "--name", "Name of the emitted bundle package.", flag_kind::value, false } },
                         { model_positional() } };
}

command_spec info_command()
{
    return command_spec{ "info", "info",
                         "Print a summary of the resolved model.",
                         { package_path_flag(),
                           { "--format", "Output format for the summary.", flag_kind::value, false } },
                         { model_positional() } };
}

command_spec validate_command()
{
    return command_spec{ "validate", "validate",
                         "Check the model and report diagnostics by class.",
                         { package_path_flag(),
                           { "--format", "Output format for the report.", flag_kind::value, false } },
                         { model_positional() } };
}

command_spec tree_command()
{
    return command_spec{ "tree", "tree",
                         "Print the link and joint topology.",
                         { package_path_flag(),
                           { "--dot", "Emit Graphviz DOT instead of ASCII.", flag_kind::boolean, false },
                           { "--root", "Render the subtree rooted at this link.", flag_kind::value, true } },
                         { model_positional() } };
}

command_spec deps_command()
{
    return command_spec{ "deps", "deps",
                         "List the assets the model references.",
                         { package_path_flag() }, { model_positional() } };
}

command_spec args_command()
{
    return command_spec{ "args", "args",
                         "List the declared arguments of the document.",
                         { package_path_flag() }, { model_positional() } };
}

command_spec resolve_command()
{
    return command_spec{ "resolve", "resolve",
                         "Resolve a link or joint to its resolved definition.",
                         { package_path_flag() },
                         { model_positional(),
                           { "target", "Link or joint name to resolve.", true } } };
}

command_spec completion_command()
{
    return command_spec{ "completion", "completion",
                         "Generate a shell completion script.",
                         {}, { { "shell", "Shell to generate a script for.", false } } };
}

}

const std::vector<command_spec> &cli_table()
{
    static const std::vector<command_spec> table = {
        flatten_command(), bundle_command(), info_command(),
        validate_command(), tree_command(), deps_command(),
        args_command(), resolve_command(), completion_command(),
    };
    return table;
}

}
