"""Lists every instruction that references an absolute address, classified.

    python xrefs.py DUMP 0x00a2946c

Use it on a candidate global. `write` rows are the places that set it (look for
the routine that sets 1 and later 0); `read` rows are what the value changes.
Only absolute references are found: a global reached through a pointer or a
getter function needs callers.py on that getter instead.
"""

import argparse
import struct

from dumplib import Dump, parse_int

WRITE_MNEMONICS = {"mov", "inc", "dec", "add", "sub", "and", "or", "xor", "not", "neg", "shl", "shr"}


def classify(insn, needle):
    operands = insn.op_str.split(",")
    first = operands[0].strip() if operands else ""
    if insn.mnemonic in WRITE_MNEMONICS and needle in first and "ptr" in first:
        return "write"
    if insn.mnemonic.startswith("set") and needle in first:
        return "write"
    if needle in insn.op_str and "ptr" in insn.op_str:
        return "read"
    return "immediate"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("dump")
    parser.add_argument("address", help="VA of the global")
    args = parser.parse_args()

    dump = Dump(args.dump)
    dump.warn_gaps()
    va = parse_int(args.address)
    if va < dump.base:
        va = dump.to_va(va)
    needle = hex(va)
    packed = struct.pack("<I", va)

    rows = []
    for start, end in dump.code_ranges():
        position = dump.data.find(packed, start, end)
        while position >= 0:
            insn = dump.instruction_covering(position, needle)
            if insn is not None:
                rva = insn.address - dump.base
                rows.append((rva, classify(insn, needle), insn, dump.function_start(rva)))
            position = dump.data.find(packed, position + 1, end)

    seen = set()
    for rva, kind, insn, function in rows:
        if rva in seen:
            continue
        seen.add(rva)
        print("%-9s rva=0x%08x func=%s  %s %s" % (
            kind, rva, hex(function) if function is not None else "?", insn.mnemonic, insn.op_str))
    counts = {kind: sum(1 for _, k, _, _ in rows if k == kind) for kind in ("write", "read", "immediate")}
    print("total=%d write=%d read=%d immediate=%d" % (len(seen), counts["write"], counts["read"], counts["immediate"]))


if __name__ == "__main__":
    main()
