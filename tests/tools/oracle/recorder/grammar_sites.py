import re
import ast

from recorder.grammar_tree import ancestors, called, is_tag_name, located, slug, text_of

PLACEHOLDER = re.compile(r"\{[^{}]*\}|%(?:\([^)]*\))?[-#0 +]*\d*(?:\.\d+)?[a-zA-Z%]")

# A named form keeps a readable id; a form literal missing here takes its derived slug.
FORM_SLUGS = {("text", "$(cwd)"): "cwd", ("text", "$(eval "): "eval_prefix",
              ("text", "xacro:eval-comments"): "eval_comments",
              ("text", "xacro:eval-comments:off"): "eval_comments_off",
              ("attribute", "xacro:targetNamespace"): "target_namespace",
              ("attribute", "xacro:"): "xacro_attribute_prefix",
              ("attribute", "xmlns:"): "xmlns_prefix", ("tag", "xacro:"): "macro_call"}


def attribute_names(node):
    function = next((one for one in ancestors(node) if isinstance(one, ast.FunctionDef)), None)
    names = set()
    for one in ast.walk(function) if function else ():
        if isinstance(one, (ast.For, ast.comprehension)) and "attributes" in ast.unparse(one.iter):
            names |= {name.id for name in ast.walk(one.target) if isinstance(name, ast.Name)}
    return names


def prefix_subject(call):
    receiver = call.func.value
    if is_tag_name(receiver):
        return "tag"
    named = isinstance(receiver, ast.Name) and receiver.id in attribute_names(call)
    return "attribute" if named else None


def call_form(node):
    literal = text_of(node.args[0]) if node.args else None
    name = called(node)
    if literal is None or name not in ("startswith", "getAttribute"):
        return None
    if name == "getAttribute":
        return ("attribute", literal) if literal.startswith("xacro:") else None
    if literal.startswith("$("):
        return ("text", literal)
    subject = prefix_subject(node) if literal.startswith(("xacro:", "xmlns:")) else None
    return (subject, literal) if subject else None


def compare_form(node):
    sides = [text_of(one) for one in [node.left, *node.comparators]]
    if any(isinstance(op, ast.Eq) for op in node.ops):
        found = [one for one in sides if one and one.startswith("$(")]
        return ("text", found[0]) if found else None
    contained = any(isinstance(op, (ast.In, ast.NotIn)) for op in node.ops)
    return ("text", sides[0]) if contained and sides[0] and sides[0].startswith("xacro:") else None


def form_rows(tree, module):
    rows = []
    for node in ast.walk(tree):
        form = call_form(node) if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute) \
            else compare_form(node) if isinstance(node, ast.Compare) else None
        if form:
            name = FORM_SLUGS.get(form, slug(form[1]))
            rows.append(located("form", name, node, module, form[1]))
    return rows


def module_texts(tree):
    return {one.targets[0].id: one.value.value for one in tree.body
            if isinstance(one, ast.Assign) and len(one.targets) == 1
            and isinstance(one.targets[0], ast.Name) and text_of(one.value) is not None}


def leading(node, texts):
    if isinstance(node, ast.JoinedStr):
        return "".join(text_of(one) or "{}" for one in node.values)
    if isinstance(node, ast.BinOp) and isinstance(node.op, (ast.Add, ast.Mod)):
        return leading(node.left, texts)
    if isinstance(node, ast.Call) and isinstance(node.func, ast.Attribute) \
            and node.func.attr == "format":
        return leading(node.func.value, texts)
    return texts.get(node.id) if isinstance(node, ast.Name) else text_of(node)


# A message opening on a placeholder ("%s: missing attribute") is named by its first words.
def stem(text):
    at = 0
    for hole in PLACEHOLDER.finditer(text or ""):
        if slug(text[at:hole.start()]):
            return text[:hole.start()]
        at = hole.end()
    return text if text and slug(text[at:]) else ""


def message(call, texts):
    text = stem(leading(call.args[0], texts) if call.args else None)
    return text, slug(PLACEHOLDER.sub(" ", text))


def raise_rows(tree, module):
    texts, rows = module_texts(tree), []
    for node in ast.walk(tree):
        if isinstance(node, ast.Raise) and isinstance(node.exc, ast.Call):
            kind = called(node.exc)
            text, name = message(node.exc, texts)
            row = located("raise", name, node, module, "{}: {}".format(kind, text))
            rows.append(row if name else row._replace(slug=slug(kind + " " + row.function)))
    return rows


def warning_rows(tree, module):
    texts, rows = module_texts(tree), []
    for node in ast.walk(tree):
        if isinstance(node, ast.Call) and called(node) == "warning":
            text, name = message(node, texts)
            row = located("warning", name, node, module, text)
            rows.append(row if name else row._replace(slug=slug(row.function)))
    return rows
