"""Finds every direct call to a function, including calls through link thunks.

    python callers.py DUMP 0x004353e0
    python callers.py DUMP 0x00401a19 --context 4

A debug build calls most functions through incremental-link `jmp` thunks, so a
search for the function address alone misses them; this resolves every E8 call
target through thunks before comparing. Sites are grouped by enclosing function,
and --context prints the instructions leading up to each call (its arguments).
"""

import argparse
import struct
from collections import defaultdict

from dumplib import Dump, parse_int


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("dump")
    parser.add_argument("function", help="VA of the function or of one of its thunks")
    parser.add_argument("--context", type=int, default=0, help="instructions to show before each call")
    args = parser.parse_args()

    dump = Dump(args.dump)
    dump.warn_gaps()
    target = dump.thunk_target(parse_int(args.function))
    print("resolved target: 0x%08x" % target)

    groups = defaultdict(list)
    thunks = set()
    for start, end in dump.code_ranges():
        data = dump.data
        for position in range(start, end - 5):
            opcode = data[position]
            if opcode not in (0xE8, 0xE9):
                continue
            destination = dump.to_va(position) + 5 + struct.unpack_from("<i", data, position + 1)[0]
            if dump.thunk_target(destination) != target:
                continue
            if opcode == 0xE9:
                thunks.add(dump.to_va(position))
                continue
            groups[dump.function_start(position)].append(position)

    if thunks:
        print("thunks: %s" % ", ".join("0x%08x" % t for t in sorted(thunks)))
    total = sum(len(sites) for sites in groups.values())
    print("call sites: %d in %d function(s)" % (total, len(groups)))
    for function, sites in sorted(groups.items(), key=lambda item: item[0] or 0):
        print("function %s: %s" % (hex(function) if function is not None else "?",
                                  ", ".join(hex(s) for s in sites)))
        if args.context:
            for site in sites:
                window = [insn for insn in dump.instructions(max(site - 0x30, 0), site + 5)]
                shown = [insn for insn in window if insn.address - dump.base <= site][-(args.context + 1):]
                for insn in shown:
                    print("    %08x  %-6s %s%s" % (insn.address - dump.base, insn.mnemonic, insn.op_str,
                                                  dump.annotate(insn)))
                print("    --")


if __name__ == "__main__":
    main()
