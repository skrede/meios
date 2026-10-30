#ifndef HPP_GUARD_MEIOS_XACRO_EVAL_FAILURE_KIND_H
#define HPP_GUARD_MEIOS_XACRO_EVAL_FAILURE_KIND_H

namespace meios
{

// Separates a construct the core cannot evaluate but a fuller backend could (unsupported, the
// only kind a lenient eval_policy may soften) from a genuine evaluation fault (error) such as
// an undefined name, from one the evaluator declines to run (refused), and from a crossed
// resource ceiling (exhausted). Every kind but unsupported is terminal under every policy.
enum class eval_failure_kind
{
    none,
    unsupported,
    error,
    refused,
    exhausted,
};

}

#endif
