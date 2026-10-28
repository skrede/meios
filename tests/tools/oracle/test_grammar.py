import unittest

from recorder.corpus import REPO
from recorder.grammar_tree import parse
from recorder.grammar import assign_ids, grammar_rows, tag_rows
from test_generated import MACHINE_PATH

RECORD = REPO / "tests" / "golden" / "oracle" / "grammar_constructs.cases"

DISPATCH = '''
def eval_all(node):
    if node.tagName == 'xacro:a':
        pass
    elif node.tagName in ['unless', 'xacro:b']:
        pass
    elif node.tagName == 'plain':
        pass
'''

UPSTREAM = r'''
LEXER = QuickLexer(EXPR=r'^\$\{[^\}]*\}', TEXT=r'x')
no_matches = "matched no files: {}"

def check_attrs(tag, required, optional):
    extra = [a for a in tag.attributes.keys() if not a.startswith('xmlns:')]
    return reqd_attrs(tag, required)

def grab(elt):
    assert elt.tagName == 'xacro:c'
    name, = reqd_attrs(elt, ['macro'])
    if name.startswith('xacro:'):
        warning("macro names must not contain prefix 'xacro:': %s" % name)

def eval_extension(s):
    if s == '$(cwd)':
        return s

def eval_all(node):
    for name, value in node.attributes.items():
        if name.startswith('xacro:'):
            node.removeAttribute(name)
    if node.tagName == 'xacro:p':
        check_attrs(node, ['name'], ['value'])
    elif node.tagName == 'xacro:c':
        grab(node)
    elif node.tagName.startswith('xacro:'):
        pass
    if "xacro:eval-comments" in node.data:
        pass
    return node.getAttribute('xacro:targetNamespace')

def resolve(key):
    raise XacroException('circular variable definition: {}'.format(key))

def setitem(k):
    warning("redefining global symbol: %s" % k)

def matched(spec):
    warning(no_matches.format(spec))

def f():
    raise XacroException('missing attribute')

def g():
    raise XacroException('missing attribute')

def handle(e):
    raise XacroException(exc=e)

def safe_eval(e):
    raise XacroException('invalid name: ' + e)

def main():
    raise RuntimeError("%s: cannot open" % main)
'''

COMMANDS = '''
def _resolve_args(arg_str):
    valid = ['find', 'env']
    return arg_str.startswith('$(eval ')
'''

SOURCES = {"__init__.py": UPSTREAM, "substitution_args.py": COMMANDS}


def derived(sources=SOURCES):
    return {row[0]: row for row in grammar_rows(sources)}


def ids(found):
    return [row[0] for row in assign_ids(found)]


class grammar_walk(unittest.TestCase):
    def test_a_tag_row_takes_only_a_prefixed_literal(self):
        self.assertEqual(ids(tag_rows(parse(DISPATCH), "m.py")), ["tag.xacro_a", "tag.xacro_b"])

    def test_a_source_without_a_dispatch_branch_refuses(self):
        with self.assertRaisesRegex(SystemExit, "no element dispatch branch; nothing was recorded$"):
            grammar_rows({"m.py": "def process(node):\n    return node\n"})

    def test_two_derivations_are_equal(self):
        self.assertEqual(grammar_rows(SOURCES), grammar_rows(SOURCES))

    def test_an_assert_on_a_tag_is_not_a_dispatch(self):
        self.assertEqual([one for one in derived() if one.startswith("tag.")],
                         ["tag.xacro_p", "tag.xacro_c"])

    def test_an_attribute_list_takes_the_tag_that_guards_it(self):
        rows = derived()
        self.assertEqual(rows["attrs.xacro_p"][4], "required=name optional=value")
        self.assertEqual(rows["attrs.xacro_c"][4], "required=macro optional=")
        self.assertEqual(len([one for one in rows if one.startswith("attrs.")]), 2)

    def test_an_unguarded_attribute_list_refuses(self):
        source = "def eval_all(node):\n    if node.tagName == 'xacro:p':\n        pass\n" \
                 "def h(n):\n    check_attrs(n, ['a'], [])\n"
        with self.assertRaisesRegex(SystemExit, "no tag guards the attribute list at m.py:5"):
            grammar_rows({"m.py": source})

    def test_a_lexer_token_quotes_its_pattern(self):
        rows = derived()
        self.assertEqual(rows["token.expr"][4], r"^\\$\\{[^\\}]*\\}")
        self.assertEqual(rows["token.text"][4], "x")

    def test_a_command_is_an_element_of_the_valid_list(self):
        self.assertEqual([one for one in derived() if one.startswith("command.")],
                         ["command.env", "command.find"])

    def test_a_form_takes_its_fixed_slug(self):
        forms = [one for one in derived() if one.startswith("form.")]
        self.assertEqual(sorted(forms), ["form.cwd", "form.eval_comments", "form.eval_prefix",
                                         "form.macro_call", "form.target_namespace",
                                         "form.xacro_attribute_prefix", "form.xmlns_prefix"])

    def test_a_message_slug_stops_at_a_placeholder(self):
        rows = derived()
        self.assertEqual(rows["raise.circular_variable_definition"][4],
                         "XacroException: circular variable definition: ")
        self.assertEqual(rows["warning.redefining_global_symbol"][4], "redefining global symbol: ")
        self.assertEqual(rows["raise.cannot_open"][4], "RuntimeError: %s: cannot open")
        self.assertIn("warning.matched_no_files", rows)

    def test_a_raise_without_a_message_takes_its_class_and_function(self):
        self.assertEqual(derived()["raise.xacroexception_handle"][4], "XacroException: ")

    def test_a_repeated_slug_takes_its_function(self):
        rows = derived()
        self.assertEqual(rows["raise.missing_attribute"][3], "__init__.py:43")
        self.assertEqual(rows["raise.missing_attribute_g"][3], "__init__.py:46")

    def test_the_scope_follows_the_enclosing_function(self):
        rows = derived()
        self.assertEqual(rows["raise.invalid_name"][2], "expression")
        self.assertEqual(rows["raise.cannot_open"][2], "tool")
        self.assertEqual(rows["raise.missing_attribute"][2], "grammar")

    def test_a_kind_the_walk_finds_no_site_of_refuses(self):
        with self.assertRaisesRegex(SystemExit, "no command site; nothing was recorded$"):
            derived({"__init__.py": UPSTREAM})


class committed_record(unittest.TestCase):
    def rows(self):
        self.assertTrue(RECORD.is_file(), "{} is not recorded".format(RECORD.name))
        lines = RECORD.read_text(encoding="utf-8").splitlines()
        return [line.split("\t") for line in lines if not line.startswith("#")]

    def test_every_row_has_five_columns(self):
        for row in self.rows():
            self.assertEqual(len(row), 5, row)

    def test_every_id_is_unique(self):
        found = [row[0] for row in self.rows()]
        self.assertEqual(len(set(found)), len(found))

    def test_every_kind_and_scope_is_named(self):
        kinds = {"tag", "attrs", "token", "command", "form", "raise", "warning"}
        for row in self.rows():
            self.assertIn(row[1], kinds, row)
            self.assertIn(row[2], {"grammar", "expression", "tool"}, row)
            self.assertTrue(row[0].startswith(row[1] + "."), row)

    def test_no_row_holds_a_machine_path(self):
        for row in self.rows():
            self.assertNotRegex("\t".join(row), MACHINE_PATH)
            self.assertNotIn(str(REPO), "\t".join(row))


if __name__ == "__main__":
    unittest.main()
