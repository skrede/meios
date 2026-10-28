import sys

from recorder.probes import DOC


def escape(text):
    return text.replace("&", "&amp;").replace("<", "&lt;").replace('"', "&quot;")


def render(tmp, body, mappings=None):
    import xacro

    source = tmp / "probe.xacro"
    source.write_text(DOC % body, encoding="utf-8", newline="\n")
    return xacro.process_file(str(source), mappings=mappings or {})


def same_length(builder, what, counted, probed):
    if counted != probed:
        sys.exit("oracle: {} produced {} {} where {} were probed; nothing was recorded"
                 .format(builder, counted, what, probed))


# The record carries the failure's first line and nothing more, because a multi-line message would
# put an embedded newline into a tab-delimited row.
def refusal_text(failure):
    lines = str(failure).splitlines()
    return lines[0] if lines and lines[0].strip() else type(failure).__name__


# Upstream wraps an evaluation failure in its own exception, so the wrapped class names the cause.
def refusal_cause(failure):
    inner = getattr(failure, "exc", None)
    return type(failure).__name__, type(inner).__name__ if inner is not None else "-"
