"""Lays an unprotected PE32 file out as its in-memory image, for the other scripts.

    python file_image.py EZ2DJ6th.EXE logs/hunt/6th.image.bin

A build without a protection layer needs no run-time dump: its sections are
already plain in the file. This copies the headers and each section to its RVA,
leaving the rest zero, and writes the same sidecar JSON `--image-dump` does
(image base, timestamp, size of image), so a file offset in the output is an
RVA. A protected build's sections are still encrypted in the file; dump it at
`resumed` instead. Keep the output under logs/, never in the repository.
"""

import json
import os
import struct
import sys


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: file_image.py EXECUTABLE OUTPUT.bin")
    source, output = sys.argv[1], sys.argv[2]
    with open(source, "rb") as handle:
        data = handle.read()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        sys.exit("%s: not a PE file" % source)
    timestamp = struct.unpack_from("<I", data, pe + 8)[0]
    count = struct.unpack_from("<H", data, pe + 6)[0]
    optional_size = struct.unpack_from("<H", data, pe + 20)[0]
    optional = pe + 24
    if struct.unpack_from("<H", data, optional)[0] != 0x10B:
        sys.exit("%s: not PE32" % source)
    image_base = struct.unpack_from("<I", data, optional + 28)[0]
    size_of_image = struct.unpack_from("<I", data, optional + 56)[0]
    size_of_headers = struct.unpack_from("<I", data, optional + 60)[0]

    image = bytearray(size_of_image)
    image[:size_of_headers] = data[:size_of_headers]
    table = optional + optional_size
    for index in range(count):
        entry = table + index * 40
        name = data[entry:entry + 8].rstrip(b"\0").decode("ascii", "replace")
        virtual_size, rva, raw_size, raw_offset = struct.unpack_from("<IIII", data, entry + 8)
        if name.lower() not in (".text", ".rdata", ".data", ".rsrc", ".reloc", ".idata", ".bss"):
            print("note: unusual section %s; a protected build needs a run-time dump" % name)
        length = min(raw_size, virtual_size) if virtual_size else raw_size
        image[rva:rva + length] = data[raw_offset:raw_offset + length]

    os.makedirs(os.path.dirname(os.path.abspath(output)), exist_ok=True)
    with open(output, "wb") as handle:
        handle.write(image)
    sidecar = {
        "image_base": "0x%08x" % image_base,
        "timestamp": "0x%08x" % timestamp,
        "size_of_image": "0x%08x" % size_of_image,
        "source": os.path.basename(source),
        "gaps": [],
    }
    with open(os.path.splitext(output)[0] + ".json", "w", encoding="utf-8") as handle:
        json.dump(sidecar, handle, indent=1)
    print("%s timestamp=0x%08x size_of_image=0x%08x" % (output, timestamp, size_of_image))


if __name__ == "__main__":
    main()
