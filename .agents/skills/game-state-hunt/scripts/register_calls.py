"""Lists every call to a registration function with its pushed arguments.

    python register_calls.py DUMP 0x00423670 --args 8

Scene-engine builds (1st SE) register each scene with one call such as
    push &handle; push "DemoGame"; push flag; push size; push cb4..cb1; call REGISTER
This prints one row per call with the arguments in call order (first argument
first) and any argument that points at a string shown as that string, which
turns the call list into a table of named scenes and their callbacks.

Only arguments passed as `push` instructions immediately before the call are
seen. Find REGISTER by reading the code that references one known scene or
object name (find_strings.py --xrefs, then disasm.py).
"""

import argparse
import struct

from dumplib import Dump, parse_int


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("dump")
    parser.add_argument("function", help="VA of the registration function or one of its thunks")
    parser.add_argument("--args", type=int, default=8, help="number of pushed arguments to show")
    args = parser.parse_args()

    dump = Dump(args.dump)
    dump.warn_gaps()
    target = dump.thunk_target(parse_int(args.function))
    rows = 0
    for start, end in dump.code_ranges():
        for position in range(start, end - 5):
            if dump.data[position] != 0xE8:
                continue
            destination = dump.to_va(position) + 5 + struct.unpack_from("<i", dump.data, position + 1)[0]
            if dump.thunk_target(destination) != target:
                continue
            window = [insn for insn in dump.instructions(max(position - 0x40, 0), position + 5)
                      if insn.address - dump.base <= position]
            pushes = [insn.op_str for insn in window if insn.mnemonic == "push"][-args.args:]
            # The last push is the first argument.
            ordered = list(reversed(pushes))
            shown = []
            for value in ordered:
                text = dump.cstr(int(value, 16)) if value.startswith("0x") else None
                shown.append('"%s"' % text if text else value)
            print("call@0x%08x  %s" % (dump.to_va(position), "  ".join(shown)))
            rows += 1
    print("calls: %d" % rows)


if __name__ == "__main__":
    main()
