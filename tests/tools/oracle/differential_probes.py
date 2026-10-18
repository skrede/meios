# The probe tables differential.py renders. Spelled exactly as the C++ comparator's own tables
# spell them, so an id names the same fixture and the same rendered file whichever side wrote it.

from recorder.corpus import REPO

# The known divergences, measured and deliberately not changed: and/or yielding a boolean rather
# than the deciding operand, a self-referential alias graph that loads here as a value containing
# itself, the two mapping-constructor argument shapes upstream accepts that are refused here, the
# four string meanings upstream carries that this grammar does not -- repetition, ordering, and
# the split's two further argument shapes -- the one mathematics name this grammar answers and
# upstream does not, and three differences in what the span scanner hands the evaluator,
# upstream's own span pattern being ^\$\{[^\}]*\} and so neither quote-aware nor nesting. None
# needs the expressions.cases seed scope -- the third reads only the document written beside the
# probe, which load_yaml resolves relative to the probe itself.
DIVERGENCE_PROBES = (("div_or_operand", "1 or 2"),
                     ("div_and_operand", "2 and 3"),
                     ("div_self_reference", "xacro.load_yaml('recursive.yaml')['a']"),
                     ("div_dict_pair_sequence", "dict([('a', 1)])"),
                     ("div_dict_mixed_arguments", "dict([('a', 1)], b=2)"),
                     ("div_string_repetition", "'ab' * 3"),
                     ("div_string_ordering", "'a' < 'b'"),
                     ("div_string_split_whitespace", "'a b'.split()"),
                     ("div_string_split_limit", "'a b c'.split(' ', 1)"),
                     ("div_absolute_value", "abs(-3)"),
                     ("div_span_closing_brace_in_literal", "'a}b'"),
                     ("div_span_format_operator", "'%.3f' % 1.2345"),
                     ("div_span_nested_expression", "${'1 + 2'}"))
RECURSIVE_YAML = "a: &a [1, *a]\n"

# A difference needing a macro, a conditional and a call site has no expression to wrap, so its
# probe is a whole committed document read from tests/fixtures/xacro/probes by both sides. Ids
# only: the document is the fixture, and repeating it here is what a rename would silently defeat.
DOCUMENT_PROBES = ("non_text_mapping_key",
                   "eval_reads_properties",
                   "argument_without_default",
                   "argument_helper_call",
                   "argument_above_declaration")
PROBE_DIR = REPO / "tests" / "fixtures" / "xacro" / "probes"

# Minimized binding documents both sides must render alike, compared whole: verdict id, fixture,
# and the caller mapping where the binding needs one. Both sides refusing counts as agreement.
AGREEMENT_PROBES = (("property_default_over_argument", "property_default_over_argument", None),
                    ("property_default_over_caller_argument", "property_default_over_argument", {"p": "C"}),
                    ("property_value_over_argument", "property_value_over_argument", None),
                    ("property_before_argument", "property_before_argument", None),
                    ("global_property_over_argument", "global_property_over_argument", None),
                    ("argument_beside_parameter", "argument_beside_parameter", None),
                    ("property_default_under_parameter", "property_default_under_parameter", None),
                    ("property_default_reads_argument", "property_default_reads_argument", None),
                    ("property_default_reads_caller_argument", "property_default_reads_argument",
                     {"cfg": "from_caller"}),
                    ("argument_by_bare_name", "argument_by_bare_name", None),
                    ("argument_command_reads_no_property", "argument_command_reads_no_property", None),
                    ("inherited_default_skips_argument", "inherited_default_skips_argument", None),
                    ("inherited_argument_refuses", "inherited_argument_refuses", None))
