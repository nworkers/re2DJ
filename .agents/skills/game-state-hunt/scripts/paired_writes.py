"""Lists the globals a function sets, directly or through one-line setters.

    python paired_writes.py DUMP 0x0048aa31

This is the step that separated 3rd's demo flag from its autoplay flag: a demo
routine turns several globals on before running the game scene and back off
afterwards. A global set to a non-zero constant and later to zero inside the
same function is marked PAIRED; those are the candidates. Globals only ever
zeroed are initialization and marked RESET.

Two write shapes are recognized: `mov dword ptr [ADDR], imm`, and `push imm;
call SETTER` where SETTER (after thunks) only stores its first argument into
one absolute address. Anything else - a value computed at run time, a pointer
member - is not seen, so an empty result does not prove the routine sets
nothing.
"""

import argparse
import re
from collections import OrderedDict

from dumplib import Dump, parse_int

DIRECT = re.compile(r"^dword ptr \[(0x[0-9a-f]+)\], (0x[0-9a-f]+|\d+)$")
STORE_ARGUMENT = re.compile(r"^dword ptr \[(0x[0-9a-f]+)\], eax$")


def setter_global(dump, function_va):
    """The address a one-argument setter stores into, or None."""
    rva = dump.to_rva(dump.thunk_target(function_va))
    loaded = False
    for index, insn in enumerate(dump.instructions(rva, rva + 0x40)):
        if index > 12 or insn.mnemonic in ("ret", "call"):
            return None
        if insn.mnemonic == "mov" and insn.op_str == "eax, dword ptr [ebp + 8]":
            loaded = True
            continue
        match = STORE_ARGUMENT.match(insn.op_str) if insn.mnemonic == "mov" else None
        if loaded and match:
            return int(match.group(1), 16)
    return None


def function_end(dump, start, limit):
    """End of the function: a `ret` followed by padding or another prologue."""
    for insn in dump.instructions(start, start + limit):
        if insn.mnemonic in ("ret", "retn"):
            after = insn.address - dump.base + insn.size
            if dump.data[after] in (0xCC, 0x90) or dump.data[after:after + 3] == b"\x55\x8b\xec":
                return after
    return start + limit


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("dump")
    parser.add_argument("function", help="VA or RVA anywhere inside the function")
    parser.add_argument("--max", type=lambda s: parse_int(s), default=0x2000)
    args = parser.parse_args()

    dump = Dump(args.dump)
    dump.warn_gaps()
    rva = dump.to_rva(parse_int(args.function))
    start = dump.function_start(rva)
    if start is None:
        start = rva
        print("; no prologue found, starting at the given address")
    end = function_end(dump, start, args.max)
    print("function 0x%08x .. 0x%08x" % (dump.to_va(start), dump.to_va(end)))

    writes = OrderedDict()
    previous = None
    setter_cache = {}
    for insn in dump.instructions(start, end):
        site = insn.address - dump.base
        if insn.mnemonic == "mov":
            match = DIRECT.match(insn.op_str)
            if match:
                writes.setdefault(int(match.group(1), 16), []).append(
                    (int(match.group(2), 0), site, "direct"))
        elif insn.mnemonic == "call" and insn.op_str.startswith("0x") and previous is not None \
                and previous.mnemonic == "push" and re.match(r"^(0x[0-9a-f]+|\d+|-?\d+)$", previous.op_str):
            callee = int(insn.op_str, 16)
            if callee not in setter_cache:
                setter_cache[callee] = setter_global(dump, callee)
            target = setter_cache[callee]
            if target is not None:
                value = int(previous.op_str, 0) & 0xFFFFFFFF
                writes.setdefault(target, []).append(
                    (value, site, "via 0x%08x" % dump.thunk_target(callee)))
        previous = insn

    if not writes:
        print("no recognized constant writes")
        return
    for address, entries in writes.items():
        values = [value for value, _, _ in entries]
        if any(values) and 0 in values[values.index(next(v for v in values if v)):]:
            tag = "PAIRED"
        elif not any(values):
            tag = "RESET"
        else:
            tag = "SET"
        detail = ", ".join("%d@0x%x(%s)" % (value, site, how) for value, site, how in entries)
        print("%-6s [0x%08x]  %s" % (tag, address, detail))


if __name__ == "__main__":
    main()
