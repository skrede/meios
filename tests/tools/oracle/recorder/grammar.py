# Derived from the syntax tree of upstream's own dispatch sites rather than listed, so a construct
# upstream adds cannot be missing from the record.

import ast
import sys
import importlib.util
from pathlib import Path

from recorder.generated import column
from recorder.environment import imported_from_pin
from recorder.grammar_sites import form_rows, raise_rows, warning_rows
from recorder.grammar_tree import ancestors, called, is_tag_name, located, parse, slug, strings

MODULES = ("__init__.py", "substitution_args.py", "xmlutils.py")
KINDS = {"tag": "element dispatch branch", "attrs": "attribute list", "token": "lexer token",
         "command": "command site", "form": "special form", "raise": "raise site",
         "warning": "warning site"}


def tag_literals(node):
    found = []
    for one in ast.walk(node):
        if isinstance(one, ast.Compare) and is_tag_name(one.left):
            found += [literal for side in one.comparators for literal in strings(side)
                      if literal.value.startswith("xacro:")]
    return found


# An assert restates the branch its caller already took, so a tag compared there is not a
# dispatch of its own.
def tag_rows(tree, module):
    return [located("tag", slug(literal.value), literal, module, literal.value)
            for literal in tag_literals(tree)
            if not any(isinstance(one, ast.Assert) for one in ancestors(literal))]


def branch_tags(node):
    if not isinstance(node, ast.If):
        return []
    return list(dict.fromkeys(literal.value for literal in tag_literals(node.test)))


# One level of helpers: a function a tag's branch calls by name is guarded by that tag.
def helper_tags(tree):
    helpers = {}
    for branch in ast.walk(tree):
        tags = branch_tags(branch)
        calls = [one for part in branch.body for one in ast.walk(part)
                 if isinstance(one, ast.Call)] if tags else []
        for call in calls:
            helpers.setdefault(called(call), []).extend(tags)
    return {name: list(dict.fromkeys(tags)) for name, tags in helpers.items()}


def guarding_tags(call, helpers):
    child = call
    for one in ancestors(call):
        if branch_tags(one) and child in one.body:
            return branch_tags(one)
        if isinstance(one, ast.FunctionDef):
            return helpers.get(one.name, [])
        child = one
    return []


def attribute_lists(tree):
    for call in ast.walk(tree):
        if isinstance(call, ast.Call) and called(call) in ("check_attrs", "reqd_attrs") \
                and len(call.args) > 1 and isinstance(call.args[1], (ast.List, ast.Tuple)):
            yield call


def listed(call, at):
    return ",".join(one.value for one in strings(call.args[at])) if len(call.args) > at else ""


def attrs_rows(tree, module):
    helpers, rows = helper_tags(tree), []
    for call in attribute_lists(tree):
        tags = guarding_tags(call, helpers)
        if not tags:
            sys.exit("oracle: no tag guards the attribute list at {}:{}; nothing was recorded"
                     .format(module, call.lineno))
        quote = "required={} optional={}".format(listed(call, 1), listed(call, 2))
        rows += [located("attrs", slug(tag), call, module, quote) for tag in tags]
    return rows


def token_rows(tree, module):
    return [located("token", slug(keyword.arg), keyword.value, module, keyword.value.value)
            for one in ast.walk(tree)
            if isinstance(one, ast.Assign) and isinstance(one.value, ast.Call)
            and called(one.value) == "QuickLexer"
            and any(isinstance(target, ast.Name) and target.id == "LEXER" for target in one.targets)
            for keyword in one.value.keywords]


def command_rows(tree, module):
    return [located("command", slug(literal.value), literal, module, literal.value)
            for one in ast.walk(tree)
            if isinstance(one, ast.Assign) and isinstance(one.value, ast.List)
            and any(isinstance(target, ast.Name) and target.id == "valid" for target in one.targets)
            for literal in strings(one.value)]


def assigned(rows, used):
    for one in rows:
        base = one.kind + "." + one.slug
        count = sum(1 for taken in used.values() if taken == base) + 1
        steps = [base] if count == 1 else []
        steps += [base + "_" + slug(one.function)] if count == 2 and slug(one.function) else []
        steps += [base + "_" + str(one.line), base + "_" + slug(one.function) + "_" + str(one.line)]
        chosen = next((step for step in steps if step not in used), None)
        if chosen is None:
            sys.exit("oracle: two sites at {}:{} derive the id {}; nothing was recorded"
                     .format(one.module, one.line, base))
        used[chosen] = base
        yield (chosen, one.kind, one.scope, "{}:{}".format(one.module, one.line), column(one.quote))


def assign_ids(rows):
    ordered = sorted(rows, key=lambda one: (one.module, one.line, one.kind + "." + one.slug,
                                            one.quote))
    return list(assigned(ordered, {}))


def grammar_rows(sources):
    walkers = (tag_rows, attrs_rows, token_rows, command_rows, form_rows, raise_rows, warning_rows)
    found = []
    for module in sorted(sources):
        tree = parse(sources[module])
        found += [one for walk in walkers for one in walk(tree, module)]
    for kind, name in KINDS.items():
        if not any(one.kind == kind for one in found):
            sys.exit("oracle: the grammar walk found no {}; nothing was recorded".format(name))
    return assign_ids(found)


def pinned_sources():
    imported_from_pin("xacro")
    package = Path(importlib.util.find_spec("xacro").origin).parent
    return {name: (package / name).read_text(encoding="utf-8") for name in MODULES}
