"""Disassembles a function or a window of a dump, annotating string operands.

    python disasm.py DUMP 0x0048aa31 --function
    python disasm.py DUMP 0x0003f9ca --before 0x20 --after 0x80

ADDRESS is a VA (at or above the image base) or an RVA. With --function the
listing starts at the enclosing `push ebp; mov ebp, esp` and ends after the
first `ret` past ADDRESS, or after --max bytes. Debug-build noise (the
0xcccccccc fill, stack checks) is dropped unless --raw is given.
"""

import argparse

from dumplib import Dump, parse_int

NOISE = ("0xcccccccc", "rep stosd")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("dump")
    parser.add_argument("address")
    parser.add_argument("--function", action="store_true")
    parser.add_argument("--before", type=lambda s: parse_int(s), default=0)
    parser.add_argument("--after", type=lambda s: parse_int(s), default=0x80)
    parser.add_argument("--max", type=lambda s: parse_int(s), default=0x600)
    parser.add_argument("--raw", action="store_true")
    args = parser.parse_args()

    dump = Dump(args.dump)
    dump.warn_gaps()
    rva = dump.to_rva(parse_int(args.address))
    if args.function:
        start = dump.function_start(rva)
        if start is None:
            print("; no frame-pointer prologue found; showing a window instead")
            start = rva - 0x40
        end = start + args.max
        stop_after = rva
    else:
        start, end, stop_after = rva - args.before, rva + args.after, None

    stack_check = None
    for insn in dump.instructions(start, end):
        here = insn.address - dump.base
        text = "%s %s" % (insn.mnemonic, insn.op_str)
        # Identify the stack-check helper as the call right after `cmp ebp, esp`.
        if not args.raw:
            if any(token in text for token in NOISE):
                continue
            if text == "cmp ebp, esp":
                stack_check = "pending"
                continue
            if stack_check == "pending" and insn.mnemonic == "call":
                stack_check = None
                continue
            stack_check = None
        marker = ">>" if here == rva else "  "
        print("%s %08x  %-6s %s%s" % (marker, here, insn.mnemonic, insn.op_str, dump.annotate(insn)))
        if stop_after is not None and here >= stop_after and insn.mnemonic in ("ret", "retn"):
            break


if __name__ == "__main__":
    main()
