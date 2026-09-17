"""Shared helpers for reading a re2DJ decrypted image dump.

A dump from `re2dj <target> --image-dump` keeps the image's virtual layout, so a
file offset is an RVA. Its sidecar JSON (same name, `.json`) records the image
base and the build, and every script here reads both.

Requires Capstone (`pip install capstone`, BSD-3-Clause). Capstone is only an
analysis dependency of these scripts; nothing in the product build uses it.
"""

import json
import os
import struct
import sys

try:
    from capstone import CS_ARCH_X86, CS_MODE_32, Cs
except ImportError:  # pragma: no cover - reported to the user
    sys.exit("capstone is required: pip install capstone")

IMAGE_SCN_MEM_EXECUTE = 0x20000000


def parse_int(text):
    """Accepts 0x-prefixed hex, plain hex with letters, or decimal."""
    text = text.strip()
    if text.lower().startswith("0x"):
        return int(text, 16)
    if any(c in "abcdefABCDEF" for c in text):
        return int(text, 16)
    return int(text, 10)


class Dump:
    def __init__(self, path):
        self.path = path
        with open(path, "rb") as handle:
            self.data = handle.read()
        sidecar = os.path.splitext(path)[0] + ".json"
        self.meta = {}
        if os.path.exists(sidecar):
            with open(sidecar, encoding="utf-8") as handle:
                self.meta = json.load(handle)
        self.base = int(self.meta.get("image_base", "0x00400000"), 16)
        self.sections = self._parse_sections()
        self._md = Cs(CS_ARCH_X86, CS_MODE_32)

    # ----- layout ---------------------------------------------------------

    def _parse_sections(self):
        d = self.data
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        if d[pe:pe + 4] != b"PE\0\0":
            sys.exit("%s: no PE header at 0x%x - not an image dump?" % (self.path, pe))
        count = struct.unpack_from("<H", d, pe + 6)[0]
        optional_size = struct.unpack_from("<H", d, pe + 20)[0]
        table = pe + 24 + optional_size
        sections = []
        for index in range(count):
            offset = table + index * 40
            name = d[offset:offset + 8].rstrip(b"\0").decode("ascii", "replace")
            virtual_size, rva = struct.unpack_from("<II", d, offset + 8)
            characteristics = struct.unpack_from("<I", d, offset + 36)[0]
            sections.append((name, rva, max(virtual_size, 1), characteristics))
        return sections

    def code_ranges(self):
        """(start_rva, end_rva) of executable sections; falls back to the first section."""
        ranges = [(rva, rva + size) for _, rva, size, flags in self.sections
                  if flags & IMAGE_SCN_MEM_EXECUTE]
        if not ranges and self.sections:
            _, rva, size, _ = self.sections[0]
            ranges = [(rva, rva + size)]
        return [(start, min(end, len(self.data))) for start, end in ranges]

    def section_of(self, rva):
        for name, start, size, _ in self.sections:
            if start <= rva < start + size:
                return name
        return "?"

    def to_rva(self, address):
        """An address at or above the image base is a VA; anything lower is an RVA."""
        return address - self.base if address >= self.base else address

    def to_va(self, rva):
        return self.base + rva

    def u32(self, rva):
        return struct.unpack_from("<I", self.data, rva)[0]

    def cstr(self, va, min_length=2, max_length=96):
        rva = va - self.base
        if not 0 <= rva < len(self.data):
            return None
        end = self.data.find(b"\0", rva, rva + max_length + 1)
        if end < 0:
            return None
        text = self.data[rva:end]
        if min_length <= len(text) and all(32 <= c < 127 for c in text):
            return text.decode("ascii")
        return None

    # ----- code -----------------------------------------------------------

    def thunk_target(self, va, depth=4):
        """Follows incremental-link `jmp rel32` thunks to the real function."""
        for _ in range(depth):
            rva = va - self.base
            if not 0 <= rva < len(self.data) - 5 or self.data[rva] != 0xE9:
                break
            va = va + 5 + struct.unpack_from("<i", self.data, rva + 1)[0]
        return va

    def function_start(self, rva, limit=0x4000):
        """Nearest `push ebp; mov ebp, esp` after padding or a return, searching back.

        A heuristic: it suits frame-pointer builds. Optimized builds without
        frames return None, and the caller should then read a window instead.
        """
        lowest = max(0, rva - limit)
        position = rva
        while position > lowest:
            if (self.data[position:position + 3] == b"\x55\x8b\xec"
                    and self.data[position - 1] in (0xCC, 0xC3, 0x90, 0xC2)):
                return position
            position -= 1
        return None

    def instructions(self, start_rva, end_rva):
        """Disassembles a range, stepping one byte past anything Capstone cannot decode.

        Capstone stops at the first invalid byte, which silently truncates a
        listing that runs into data or padding; resynchronizing avoids that.
        """
        position = start_rva
        end_rva = min(end_rva, len(self.data))
        while position < end_rva:
            decoded = False
            for insn in self._md.disasm(self.data[position:end_rva], self.base + position):
                decoded = True
                yield insn
                position = insn.address - self.base + insn.size
            if not decoded:
                position += 1

    def instruction_covering(self, operand_rva, needle):
        """Decodes the instruction whose operand bytes start at operand_rva.

        Two passes, longest back-off first in each. A short back-off decodes
        the middle of the real instruction as something else (a `cmp dword ptr
        [addr], 0` read one byte late becomes `cmp eax, addr`), but a long one
        can swallow the previous instruction's last byte and invent a
        register-indexed form (`mov [addr], eax` became `or byte ptr [ebx +
        addr], ah`). So a pure absolute operand `[addr]` is preferred first,
        and any decoding mentioning the address is accepted only after that.
        """
        absolute = "[" + needle + "]"
        for accept in (lambda op: absolute in op, lambda op: needle in op):
            for back in range(7, 0, -1):
                start = operand_rva - back
                if start < 0:
                    continue
                insn = next(self._md.disasm(self.data[start:start + 15], self.base + start), None)
                if insn is not None and insn.size >= back + 4 and accept(insn.op_str):
                    return insn
        return None

    def annotate(self, insn):
        """Appends the string an immediate operand points at, when there is one."""
        tokens = insn.op_str.replace("[", " ").replace("]", " ").replace(",", " ").split()
        for token in tokens:
            if token.startswith("0x"):
                text = self.cstr(int(token, 16))
                if text:
                    return '  ; "%s"' % text
        return ""

    def warn_gaps(self):
        gaps = self.meta.get("gaps") or []
        if gaps:
            print("warning: %d unread range(s) are zero-filled in this dump" % len(gaps), file=sys.stderr)
        point = self.meta.get("point")
        if point and point != "resumed":
            print("warning: dump point is '%s'; protected builds are still packed there - use the resumed dump"
                  % point, file=sys.stderr)
