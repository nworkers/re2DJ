"""Reads, polls, or writes 32-bit values in a running guest process.

    python guest_memory.py read  --process EZ2DJ.EXE 0x00a2946c 0x00a29508
    python guest_memory.py poll  --process EZ2DJ.EXE --seconds 150 demo=0x00a2946c autoplay=0x00a29508
    python guest_memory.py write --process EZ2DJ.EXE 0x00a29508 1 --yes

`read` and `poll` open the process for reading only. `write` changes the guest's
state and refuses to run without --yes: it is an intervention, done only with
the user's agreement and while the user drives the game, and every write is read
back. Addresses are VAs in the guest; they belong to one build, so check the
dump sidecar's timestamp matches the executable being run.
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import subprocess
import sys
import time

PROCESS_VM_READ = 0x0010
PROCESS_VM_WRITE = 0x0020
PROCESS_VM_OPERATION = 0x0008
PROCESS_QUERY_INFORMATION = 0x0400

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
kernel32.OpenProcess.restype = wt.HANDLE
kernel32.ReadProcessMemory.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t,
                                       ctypes.POINTER(ctypes.c_size_t)]
kernel32.WriteProcessMemory.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t,
                                        ctypes.POINTER(ctypes.c_size_t)]


def parse_int(text):
    return int(text, 16) if text.lower().startswith("0x") else int(text, 10)


def find_pid(image_name, wait_seconds):
    deadline = time.time() + wait_seconds
    while True:
        output = subprocess.run(["tasklist", "/FI", "IMAGENAME eq %s" % image_name, "/FO", "CSV", "/NH"],
                                capture_output=True, text=True).stdout
        for line in output.splitlines():
            fields = line.split('","')
            if len(fields) > 1 and fields[0].strip('"').lower() == image_name.lower():
                return int(fields[1].strip('"'))
        if time.time() >= deadline:
            return None
        time.sleep(0.5)


def read_u32(handle, address):
    value = ctypes.c_uint32()
    count = ctypes.c_size_t()
    if not kernel32.ReadProcessMemory(handle, address, ctypes.byref(value), 4, ctypes.byref(count)):
        raise OSError("read failed at 0x%08x (error %d)" % (address, ctypes.get_last_error()))
    return value.value


def write_u32(handle, address, number):
    value = ctypes.c_uint32(number)
    count = ctypes.c_size_t()
    if not kernel32.WriteProcessMemory(handle, address, ctypes.byref(value), 4, ctypes.byref(count)):
        raise OSError("write failed at 0x%08x (error %d)" % (address, ctypes.get_last_error()))


def named(entries):
    result = []
    for entry in entries:
        name, _, address = entry.rpartition("=")
        result.append((name or address, parse_int(address)))
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("command", choices=["read", "poll", "write"])
    parser.add_argument("values", nargs="+", help="read/poll: [name=]ADDRESS ...; write: ADDRESS VALUE")
    parser.add_argument("--process", default="EZ2DJ.EXE")
    parser.add_argument("--wait", type=float, default=30.0, help="seconds to wait for the process to appear")
    parser.add_argument("--seconds", type=float, default=60.0, help="poll duration")
    parser.add_argument("--interval", type=float, default=0.25)
    parser.add_argument("--yes", action="store_true", help="confirm a write")
    args = parser.parse_args()

    pid = find_pid(args.process, args.wait)
    if pid is None:
        sys.exit("%s is not running" % args.process)

    if args.command == "write":
        if len(args.values) != 2:
            sys.exit("write takes ADDRESS VALUE")
        if not args.yes:
            sys.exit("refusing to write without --yes: this changes the running guest")
        access = PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION
    else:
        access = PROCESS_VM_READ | PROCESS_QUERY_INFORMATION
    handle = kernel32.OpenProcess(access, False, pid)
    if not handle:
        sys.exit("cannot open pid %d (error %d)" % (pid, ctypes.get_last_error()))

    if args.command == "write":
        address, number = parse_int(args.values[0]), parse_int(args.values[1])
        before = read_u32(handle, address)
        write_u32(handle, address, number)
        time.sleep(0.1)
        print("%s pid=%d 0x%08x: %d -> %d" % (time.strftime("%H:%M:%S"), pid, address, before,
                                            read_u32(handle, address)))
        return

    entries = named(args.values)
    if args.command == "read":
        print("%s pid=%d %s" % (time.strftime("%H:%M:%S"), pid,
                               " ".join("%s=%d" % (n, read_u32(handle, a)) for n, a in entries)))
        return

    print("pid=%d polling %d value(s) for %.0fs; a line is printed only when something changes" %
          (pid, len(entries), args.seconds))
    start = time.time()
    last = None
    while time.time() - start < args.seconds:
        try:
            values = tuple(read_u32(handle, a) for _, a in entries)
        except OSError as error:
            print("t=%7.2fs %s (process may have exited)" % (time.time() - start, error))
            return
        if values != last:
            print("t=%7.2fs %s" % (time.time() - start,
                                  " ".join("%s=%d" % (n, v) for (n, _), v in zip(entries, values))))
            sys.stdout.flush()
            last = values
        time.sleep(args.interval)


if __name__ == "__main__":
    main()
