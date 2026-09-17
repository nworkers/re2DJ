"""Finds strings in a dump and, optionally, the code that references each one.

    python find_strings.py DUMP "demo|auto|attract" --xrefs
    python find_strings.py DUMP "^TotalCoin$" --exact --xrefs

The pattern is a case-insensitive regular expression matched against every
printable ASCII run of four or more characters. A reference is an absolute
32-bit address of the string's first byte inside an executable section.
"""

import argparse
import re
import struct
from collections import defaultdict

from dumplib import Dump


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("dump")
    parser.add_argument("pattern")
    parser.add_argument("--xrefs", action="store_true", help="list code references to each string")
    parser.add_argument("--exact", action="store_true", help="match only NUL-terminated whole strings")
    parser.add_argument("--limit", type=int, default=200)
    args = parser.parse_args()

    dump = Dump(args.dump)
    dump.warn_gaps()
    pattern = re.compile(args.pattern, re.IGNORECASE)
    code = dump.code_ranges()

    references = defaultdict(list)
    if args.xrefs:
        for start, end in code:
            data = dump.data
            for position in range(start, end - 3):
                references[struct.unpack_from("<I", data, position)[0]].append(position)

    shown = 0
    for match in re.finditer(rb"[\x20-\x7e]{4,}", dump.data):
        text = match.group().decode("ascii")
        rva = match.start()
        if args.exact:
            if dump.data[rva - 1:rva] not in (b"\0", b"") or dump.data[match.end():match.end() + 1] != b"\0":
                continue
        if not pattern.search(text):
            continue
        va = dump.to_va(rva)
        line = "rva=0x%08x va=0x%08x [%s] %r" % (rva, va, dump.section_of(rva), text[:80])
        if args.xrefs:
            sites = references.get(va, [])
            functions = sorted({dump.function_start(site) or site for site in sites})
            line += "  refs=%s functions=%s" % ([hex(s) for s in sites[:8]], [hex(f) for f in functions[:8]])
        print(line)
        shown += 1
        if shown >= args.limit:
            print("... limit reached")
            break
    if shown == 0:
        print("no match. A protected build's entry dump or disk file has no strings - use the resumed dump.")


if __name__ == "__main__":
    main()
