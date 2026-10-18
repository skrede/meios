import re
import sys

from recorder.rendering import render, escape, refusal_text
from recorder.probes import (SEEDS, LIMIT_SEEDS, SEED_YAML, SEQUENCE_YAML, COLLISION_YAML,
                             SEQUENCE_PROBES, COLLISION_PROBES, CONSTRUCTOR_PROBES, STRING_PROBES,
                             GRAMMAR_PROBES, MATH_PROBES, MEMBER_PROBES)


def closure_expressions(share):
    seen = []
    for name in ("urdf/ur.urdf.xacro", "urdf/ur_macro.xacro", "urdf/inc/ur_common.xacro"):
        for found in re.finditer(r"\$\{([^{}]*)\}", (share / name).read_text(encoding="utf-8")):
            if found.group(1) not in seen:
                seen.append(found.group(1))
    trivial = re.compile(r"^[A-Za-z_]\w*(\['[^']*'\])*$")
    return [one for one in seen if not trivial.match(one)]


def seed_body(seed_file, sequence_file, collision_file):
    rows = [' <xacro:property name="seed_file" value="%s"/>' % escape(str(seed_file)),
            ' <xacro:property name="sequence_file" value="%s"/>' % escape(str(sequence_file)),
            ' <xacro:property name="collision_file" value="%s"/>' % escape(str(collision_file))]
    rows += [' <xacro:property name="%s_parameters_file" value="%s"/>' % (n, escape(str(seed_file)))
             for n in ("joint_limits", "kinematics", "physical", "visual")]
    rows += [' <xacro:property name="%s" value="%s"/>' % (n, escape(v)) for n, v in SEEDS.items()]
    for at, joint in enumerate(LIMIT_SEEDS):
        rows += [' <xacro:property name="%s_%s_limit" value="%s"/>' % (joint, edge, sign * (6.0 + at))
                 for edge, sign in (("lower", -1.0), ("upper", 1.0))]
    return "\n".join(rows)


def write_seed_documents(tmp):
    (tmp / "seed.yaml").write_text(SEED_YAML, encoding="utf-8", newline="\n")
    (tmp / "sequence.yaml").write_text(SEQUENCE_YAML, encoding="utf-8", newline="\n")
    (tmp / "collisions.yaml").write_text(COLLISION_YAML, encoding="utf-8", newline="\n")
    return seed_body(tmp / "seed.yaml", tmp / "sequence.yaml", tmp / "collisions.yaml")


# A run in which the environment rather than the expression produced the failures would otherwise
# be recorded as a corpus of expected refusals, and the digest gate would then certify it. The floor
# leaves room for a genuine future refusal without admitting a wholesale failure.
REFUSAL_FLOOR = 2


def closure_cases(share):
    return [("e{:02d}".format(at + 1), one)
            for at, one in enumerate(closure_expressions(share))]


def sequence_cases():
    return [("s{:02d}".format(at + 1), one) for at, one in enumerate(SEQUENCE_PROBES)]


def collision_cases():
    return [("c{:02d}".format(at + 1), one) for at, one in enumerate(COLLISION_PROBES)]


def constructor_cases():
    return [("d{:02d}".format(at + 1), one) for at, one in enumerate(CONSTRUCTOR_PROBES)]


def string_cases():
    return [("t{:02d}".format(at + 1), one) for at, one in enumerate(STRING_PROBES)]


def grammar_cases():
    return [("n{:02d}".format(at + 1), one) for at, one in enumerate(GRAMMAR_PROBES)]


def math_cases():
    return [("m{:02d}".format(at + 1), one) for at, one in enumerate(MATH_PROBES)]


def member_cases():
    return [("p{:02d}".format(at + 1), one) for at, one in enumerate(MEMBER_PROBES)]


def authored_cases():
    return (sequence_cases() + collision_cases() + constructor_cases() + string_cases()
            + grammar_cases() + math_cases() + member_cases())


def expression_cases(share):
    return closure_cases(share) + authored_cases()


def probe_row(tmp, seeds, case_id, expression):
    body = '%s\n <e v="${%s}"/>' % (seeds, escape(expression))
    try:
        text = render(tmp, body).getElementsByTagName("e")[0].getAttribute("v")
        return (case_id, expression, text, "")
    except Exception as failure:
        return (case_id, expression, "REFUSED", refusal_text(failure))


def refused_in(rows):
    return sum(1 for one in rows if one[2] == "REFUSED")


# The closure floor and the authored floor bound the same failure from opposite ends: the closure
# is expected to render, so too many refusals is a broken environment, while the authored probes
# exist to record refusals, so a set in which nothing rendered is one.
def expression_rows(tmp, share):
    seeds = write_seed_documents(tmp)
    closure = [probe_row(tmp, seeds, one, text) for one, text in closure_cases(share)]
    if refused_in(closure) > REFUSAL_FLOOR:
        sys.exit("oracle: {} expressions refused where at most {} is a measurement rather than a "
                 "broken environment; nothing was recorded"
                 .format(refused_in(closure), REFUSAL_FLOOR))
    authored = [probe_row(tmp, seeds, one, text) for one, text in authored_cases()]
    if refused_in(authored) == len(authored):
        sys.exit("oracle: every authored subscript probe refused, which is a broken environment "
                 "rather than a measurement; nothing was recorded")
    return closure + authored

