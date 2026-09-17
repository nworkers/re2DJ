"""Lists every INI key bound to a settings object.

    python settings_registry.py DUMP 0x005111e0

EZ2DJ 3rd and 4th bind each key as
    push <variable address or value>; push "<key>"; mov ecx, [SETTINGS]; call <accessor>
Find SETTINGS by reading the code around one known key (disasm.py), then run
this to get the complete set. If the set equals the keys in the target's INI
file and nothing like autoplay is among them, autoplay is not a stored setting.

A key string with no code reference but present in the dump is usually the
game's run-time copy of what it read from the INI file, not a key it knows.
"""

import argparse
import struct
from collections import OrderedDict

from dumplib import Dump, parse_int


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("dump")
    parser.add_argument("settings", help="VA of the global holding the settings object pointer")
    args = parser.parse_args()

    dump = Dump(args.dump)
    dump.warn_gaps()
    settings = parse_int(args.settings)
    load = b"\x8b\x0d" + struct.pack("<I", settings)  # mov ecx, dword ptr [SETTINGS]

    keys = OrderedDict()
    for start, end in dump.code_ranges():
        position = dump.data.find(load, start, end)
        while position >= 0:
            call = position + 6
            if dump.data[call] == 0xE8 and dump.data[position - 5] == 0x68:
                key = dump.cstr(struct.unpack_from("<I", dump.data, position - 4)[0])
                if key:
                    callee = dump.to_va(call) + 5 + struct.unpack_from("<i", dump.data, call + 1)[0]
                    if dump.data[position - 10] == 0x68:
                        argument = "0x%08x" % struct.unpack_from("<I", dump.data, position - 9)[0]
                    else:
                        argument = "(computed)"
                    entry = keys.setdefault(key, {"accessors": set(), "arguments": set(), "sites": []})
                    entry["accessors"].add(dump.thunk_target(callee))
                    entry["arguments"].add(argument)
                    entry["sites"].append(position)
            position = dump.data.find(load, position + 1, end)

    print("settings object global 0x%08x: %d key(s)" % (settings, len(keys)))
    for key, entry in keys.items():
        print("%-24s accessors=%s args=%s sites=%d" % (
            key,
            ",".join("0x%08x" % a for a in sorted(entry["accessors"])),
            ",".join(sorted(entry["arguments"])),
            len(entry["sites"])))


if __name__ == "__main__":
    main()
