import re
import ast
from typing import NamedTuple

# The functions and classes of upstream's ${} expression surface.
EXPRESSION = frozenset({"YamlListWrapper", "YamlDictWrapper", "ConstructUnits", "load_yaml",
                        "create_global_symbols", "safe_eval"})
TOOL = frozenset({"open_output", "print_location", "_process", "process", "main"})


class construct(NamedTuple):
    kind: str
    slug: str
    scope: str
    module: str
    line: int
    function: str
    quote: str


def parse(source):
    tree = ast.parse(source)
    for node in ast.walk(tree):
        for child in ast.iter_child_nodes(node):
            child.parent = node
    return tree


def ancestors(node):
    while (node := getattr(node, "parent", None)) is not None:
        yield node


def enclosing(node):
    functions = [one.name for one in ancestors(node)
                 if isinstance(one, (ast.FunctionDef, ast.AsyncFunctionDef))]
    classes = [one.name for one in ancestors(node) if isinstance(one, ast.ClassDef)]
    return functions, classes


def scope_of(function, owner):
    if function in EXPRESSION or owner in EXPRESSION:
        return "expression"
    return "tool" if function in TOOL else "grammar"


def slug(text):
    return re.sub(r"[^a-z0-9]+", "_", text.lower()).strip("_")


def located(kind, name, node, module, quote, line=None):
    functions, classes = enclosing(node)
    outer = functions[-1] if functions else ""
    scope = scope_of(outer, classes[-1] if classes else "")
    return construct(kind, name, scope, module, line or node.lineno,
                     functions[0] if functions else "", quote)


def text_of(node):
    return node.value if isinstance(node, ast.Constant) and isinstance(node.value, str) else None


def strings(node):
    if isinstance(node, (ast.List, ast.Tuple)):
        return [one for element in node.elts for one in strings(element)]
    return [node] if text_of(node) is not None else []


def called(call):
    if isinstance(call.func, ast.Name):
        return call.func.id
    return call.func.attr if isinstance(call.func, ast.Attribute) else None


def is_tag_name(node):
    return isinstance(node, ast.Attribute) and node.attr == "tagName"
