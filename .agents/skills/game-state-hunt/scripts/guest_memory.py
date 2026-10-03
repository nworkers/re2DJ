"""Reads, polls, or writes 32-bit values in a running guest process.

    python guest_memory.py read  --process EZ2DJ.EXE 0x00a2946c 0x00a29508
    python guest_memory.py poll  --process EZ2DJ.EXE --seconds 150 demo=0x00a2946c autoplay=0x00a29508
    python guest_memory.py write --process EZ2DJ.EXE 0x00a29508 1 --yes

On Linux the guest runs inside a re2dj host process, so --process matches a
substring of the process's command line (a child run carries its executable,
for example EZ2DJ6TH.EXE), and memory is reached through /proc/<pid>/mem at the
guest's own addresses. Yama's ptrace_scope 1 lets only an ancestor read it, so
--launch starts the run from this script:

    python guest_memory.py poll --process EZ2DJ6TH.EXE --seconds 150 \
        --launch "build/linux-x64-debug/bin/re2dj --hdd roms/ez2dj6th --target ez2dj6th --run" \
        demo=0x008895f8 autoplay=0x008896ac

`read` and `poll` open the process for reading only. `write` changes the guest's
state and refuses to run without --yes: it is an intervention, done only with
the user's agreement and while the user drives the game, and every write is read
back. Addresses are VAs in the guest; they belong to one build, so check the
dump sidecar's timestamp matches the executable being run.
"""

import argparse
import os
import shlex
import struct
import subprocess
import sys
import time

WINDOWS = sys.platform == "win32"

if WINDOWS:
    import ctypes
    import ctypes.wintypes as wt

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


def find_pid_windows(image_name):
    output = subprocess.run(["tasklist", "/FI", "IMAGENAME eq %s" % image_name, "/FO", "CSV", "/NH"],
                            capture_output=True, text=True).stdout
    for line in output.splitlines():
        fields = line.split('","')
        if len(fields) > 1 and fields[0].strip('"').lower() == image_name.lower():
            return int(fields[1].strip('"'))
    return None


def parent_of(pid):
    try:
        with open("/proc/%d/status" % pid) as handle:
            for line in handle:
                if line.startswith("PPid:"):
                    return int(line.split()[1])
    except OSError:
        pass
    return 0


def ancestors_of(pid):
    result = set()
    while pid > 1:
        pid = parent_of(pid)
        result.add(pid)
    return result


def find_pid_linux(pattern, root):
    # The newest match wins: a launcher's child is started after its parent,
    # and a later child replaces an ended one. With --launch only that run's
    # processes count; otherwise this script's own ancestors are skipped, as
    # the shell that started it carries the pattern on its command line too.
    excluded = ancestors_of(os.getpid()) | {os.getpid()}
    found = None
    for entry in os.listdir("/proc"):
        if not entry.isdigit() or int(entry) in excluded:
            continue
        pid = int(entry)
        if root is not None and pid != root and root not in ancestors_of(pid):
            continue
        try:
            with open("/proc/%d/cmdline" % pid, "rb") as handle:
                arguments = handle.read().split(b"\0")
        except OSError:
            continue
        if any(pattern.encode() in argument for argument in arguments[1:]):
            if found is None or pid > found:
                found = pid
    return found


def find_pid(name, wait_seconds, root=None):
    deadline = time.time() + wait_seconds
    while True:
        pid = find_pid_windows(name) if WINDOWS else find_pid_linux(name, root)
        if pid is not None:
            return pid
        if time.time() >= deadline:
            return None
        time.sleep(0.5)


class WindowsProcess:
    def __init__(self, pid, write):
        access = PROCESS_VM_READ | PROCESS_QUERY_INFORMATION
        if write:
            access |= PROCESS_VM_WRITE | PROCESS_VM_OPERATION
        self.handle = kernel32.OpenProcess(access, False, pid)
        if not self.handle:
            sys.exit("cannot open pid %d (error %d)" % (pid, ctypes.get_last_error()))

    def read_u32(self, address):
        value = ctypes.c_uint32()
        count = ctypes.c_size_t()
        if not kernel32.ReadProcessMemory(self.handle, address, ctypes.byref(value), 4, ctypes.byref(count)):
            raise OSError("read failed at 0x%08x (error %d)" % (address, ctypes.get_last_error()))
        return value.value

    def write_u32(self, address, number):
        value = ctypes.c_uint32(number)
        count = ctypes.c_size_t()
        if not kernel32.WriteProcessMemory(self.handle, address, ctypes.byref(value), 4, ctypes.byref(count)):
            raise OSError("write failed at 0x%08x (error %d)" % (address, ctypes.get_last_error()))


class LinuxProcess:
    def __init__(self, pid, write):
        try:
            self.handle = open("/proc/%d/mem" % pid, "r+b" if write else "rb", buffering=0)
        except OSError as error:
            sys.exit("cannot open pid %d memory (%s); with ptrace_scope 1 start the run with --launch"
                     % (pid, error))

    def read_u32(self, address):
        self.handle.seek(address)
        data = self.handle.read(4)
        if len(data) != 4:
            raise OSError("read failed at 0x%08x" % address)
        return struct.unpack("<I", data)[0]

    def write_u32(self, address, number):
        self.handle.seek(address)
        if self.handle.write(struct.pack("<I", number)) != 4:
            raise OSError("write failed at 0x%08x" % address)


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
    parser.add_argument("--process", default="EZ2DJ.EXE",
                        help="Windows: image name; Linux: a substring of the command line")
    parser.add_argument("--launch", help="start this command first, as this script's child")
    parser.add_argument("--wait", type=float, default=30.0, help="seconds to wait for the process to appear")
    parser.add_argument("--seconds", type=float, default=60.0, help="poll duration")
    parser.add_argument("--interval", type=float, default=0.25)
    parser.add_argument("--yes", action="store_true", help="confirm a write")
    args = parser.parse_args()

    if args.command == "write":
        if len(args.values) != 2:
            sys.exit("write takes ADDRESS VALUE")
        if not args.yes:
            sys.exit("refusing to write without --yes: this changes the running guest")

    launched = subprocess.Popen(shlex.split(args.launch)) if args.launch else None
    try:
        run(args, launched.pid if launched is not None else None)
    finally:
        if launched is not None:
            stop_tree(launched)


def stop_tree(launched):
    # A launcher waits on its child, so the child goes first.
    if not WINDOWS:
        for entry in os.listdir("/proc"):
            if entry.isdigit() and launched.pid in ancestors_of(int(entry)):
                try:
                    os.kill(int(entry), 15)
                except OSError:
                    pass
    if launched.poll() is None:
        launched.terminate()
    launched.wait()


def run(args, root):
    pid = find_pid(args.process, args.wait, root)
    if pid is None:
        sys.exit("%s is not running" % args.process)
    write = args.command == "write"
    process = WindowsProcess(pid, write) if WINDOWS else LinuxProcess(pid, write)
    # A process found as it starts may not have mapped the guest image yet.
    first = parse_int(args.values[0].rpartition("=")[2])
    deadline = time.time() + args.wait
    while True:
        try:
            process.read_u32(first)
            break
        except OSError:
            if time.time() >= deadline:
                raise
            time.sleep(0.5)

    if write:
        address, number = parse_int(args.values[0]), parse_int(args.values[1])
        before = process.read_u32(address)
        process.write_u32(address, number)
        time.sleep(0.1)
        print("%s pid=%d 0x%08x: %d -> %d" % (time.strftime("%H:%M:%S"), pid, address, before,
                                            process.read_u32(address)))
        return

    entries = named(args.values)
    if args.command == "read":
        print("%s pid=%d %s" % (time.strftime("%H:%M:%S"), pid,
                               " ".join("%s=%d" % (n, process.read_u32(a)) for n, a in entries)))
        return

    print("pid=%d polling %d value(s) for %.0fs; a line is printed only when something changes" %
          (pid, len(entries), args.seconds))
    sys.stdout.flush()
    start = time.time()
    last = None
    while time.time() - start < args.seconds:
        try:
            values = tuple(process.read_u32(a) for _, a in entries)
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
