DOC = ('<?xml version="1.0"?>\n<robot name="probe" xmlns:xacro="http://wiki.ros.org/xacro">\n'
       '%s\n</robot>\n')
FIXED_PROBES = ("-0.0005", "-0.0001", "0.0004", "0.001", "0.0001", "0.00001", "1e15", "1e16",
                "1e17", "180.0", "0.0", "-0.0")
SCALARS = ("true", "false", "yes", "no", "y", "n", "'true'", '"1"', "1e5", "1.5e3", "1.0e+5",
           "010", "1_000", "42", "3.5", "", "~", "null", "hello")
SEED_YAML = ("mesh_files:\n  base:\n    visual:\n      mesh:\n        package: ur_description\n"
             "        path: meshes/base.dae\njoint_limits:\n  shoulder_pan:\n    min: -6.28\n")
# A second document, kept apart from the seed above so adding a sequence does not change what the
# expressions reading the whole seed document already render.
SEQUENCE_YAML = "bounds:\n  - -6.28\n  - 0.0\n  - 6.28\n"
# A third, kept apart for the same reason: two keys the mapping wrapper answers itself and one it
# does not, so the subscript spelling of each can be rendered beside the other.
COLLISION_YAML = "keys: 5\nvalues: 6\nplain: 7\n"
# Upstream registers exactly these six tags and converts each by multiplying the tagged text by
# its constant. An operand of 1 makes a row carry the constant itself; the wider operands separate
# the constants from one another and pin the multiplication order.
UNIT_TAGS = (("!radians", ("1", "0.5", "-2")),
             ("!degrees", ("1", "45", "90", "-180.0")),
             ("!meters", ("1", "2", "-0.75")),
             ("!millimeters", ("1", "1500", "-25.4")),
             ("!foot", ("1", "3", "-0.5")),
             ("!inches", ("1", "12", "-6")))
SEEDS = {"safety_pos_margin": "0.15", "mass": "3.7", "radius": "0.06", "length": "0.12",
         "wrist_3_joint_type": "continuous", "name": "base", "type": "visual",
         "sec_mesh_files": "${xacro.load_yaml(seed_file)['mesh_files']}",
         "sec_bounds": "${xacro.load_yaml(sequence_file)['bounds']}",
         "sec_collisions": "${xacro.load_yaml(collision_file)}"}
# Authored rather than found in the closure: no pinned description subscripts a sequence, so the
# spellings are named here and upstream is asked what each one means.
SEQUENCE_PROBES = ("sec_bounds[0]", "sec_bounds[-1]", "sec_bounds[3]", "sec_bounds[-4]",
                   "sec_bounds['0']")
# The subscript spelling of a name the mapping wrapper answers itself, beside a name it does not.
# Only this spelling is probed: the attribute spelling of a colliding name renders an object
# address and does not reproduce between runs.
COLLISION_PROBES = ("sec_collisions['keys']", "sec_collisions['values']",
                    "sec_collisions['plain']")
# The keyword-argument mapping constructor, driven through a minimized document rather than read
# out of a description: the two pinned files carrying the form are macro fragments, and loading a
# fragment as a document turns every refusal into a false one. The refusing probes are the ones
# whose refusal upstream names something an independent implementation can also name -- a repeated
# keyword, an absent key, an argument owing its value.
CONSTRUCTOR_PROBES = ("dict(a=1, b=2)", "dict(a=1, b=2)['a']", "dict()",
                      "dict(radius=radius, length=length)['radius']", "dict(a=dict(b=2)['b'])",
                      "dict(mesh=sec_mesh_files['base'])", "'a' in dict(a=1)",
                      "dict()['missing']", "dict(a=1, a=2)", "dict(a=)")
# The four string meanings a real vendor description evaluates on its default render, driven
# through a minimized document because the pinned files carrying them are macro fragments: two
# strings added, a string's truth value, substring containment, and the one named split. The
# refusing probes are the ones whose refusal upstream names something an independent
# implementation can also name -- an operand mismatch, a needle that is not text, an index past
# the last field, an empty separator, and a separator that is not text.
STRING_PROBES = ("'x' + 'y'", "'' + 'y'", "'x' + ''", "'' + ''", "name + '_' + type",
                 "'x' + 1", "1 + 'x'", "'x' - 'y'", "'x' / 'y'", "'x' // 'y'", "'x' % 'y'",
                 "'x' ** 'y'",
                 "not ''", "not 'a'", "'' if '' else name + type + '_'",
                 "name + '_' if name else ''", "'' if name else name + type + '_'",
                 "'x' in 'axb'", "'' in 'abc'", "'z' in 'abc'", "1 in 'abc'",
                 "'a b c'.split(' ')", "'a b c'.split(' ')[0]", "'a b c'.split(' ')[-1]",
                 "'a  b'.split(' ')", "' a '.split(' ')", "''.split(' ')", "'abc'.split('x')",
                 "'ab'.split('b')", "'a b c'.split(' ')[3]", "'abc'.split('')",
                 "'a b'.split(1)",
                 "sec_mesh_files['base']['visual']['mesh']['path'].split('/')[-1]")
# The arithmetic, comparison and logical spellings the grammar admits that no pinned description
# writes and no family above reaches, each reduced to the smallest expression exercising one of
# them. Three groups earn their place beyond the bare spelling: an integer operation whose result
# stays an integer beside a division whose result does not, a floor and a remainder taken over a
# negative operand, and a logical operator whose second operand names nothing -- the last being
# the only spelling that says out loud whether the operand was evaluated.
GRAMMAR_PROBES = ("42", "3.5", "'x'", "True", "False", "radius", "pi", "(1 + 2) * 3",
                  "-42", "+42", "+3.5",
                  "2 + 3", "5 - 3", "3 * 4", "7 / 2",
                  "7 // 2", "-7 // 2", "7.5 // 2",
                  "7 % 2", "-7 % 2", "7.5 % 2",
                  "2 ** 10", "2 ** -1",
                  "1 < 2", "1 <= 1", "2 > 1", "1 >= 2", "1 == 1", "1 != 2",
                  "1 < 2 < 3", "1 < 3 < 2",
                  "True and False", "False and undefined_probe_name",
                  "True or False", "True or undefined_probe_name",
                  "1 / 0")
# The mathematics the grammar calls by bare name. `abs` is not here: upstream's expression
# globals carry the mathematics module and the two reducers and no builtins at all, so `abs`
# refuses there and renders here. That is a difference in what each side accepts rather than in
# what either renders, so it is carried by the divergence manifest and not by a row here.
MATH_PROBES = ("acos(1)", "asin(0)", "atan(1)", "atan2(1, 1)", "ceil(1.2)", "cos(0)",
               "degrees(pi)", "floor(1.8)", "max(1, 2, 3)", "min(1, 2, 3)", "radians(180)",
               "sin(0)", "sqrt(4)", "tan(0)")
# The dotted read of a key in a loaded document. Every other document read recorded here is
# spelled as a subscript, so without these the one spelling upstream answers through its wrapper's
# attribute lookup rather than through its subscript would be unmeasured.
MEMBER_PROBES = ("sec_mesh_files.base", "sec_mesh_files.base.visual", "sec_mesh_files.missing")

LIMIT_SEEDS = ("shoulder_pan", "shoulder_lift", "elbow_joint", "wrist_1", "wrist_2", "wrist_3")
