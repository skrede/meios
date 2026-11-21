# Stands in for an interpreter that does not let a program set its integer digit limit.
import sys


def refuse(limit):
    raise RuntimeError("this interpreter keeps its own integer digit limit")


sys.set_int_max_str_digits = refuse
