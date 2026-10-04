"""Reads, polls, or writes 32-bit values in a running guest process.

    python guest_memory.py read  --process ez2dj3rd 0x00a2946c 0x00a29508
    python guest_memory.py poll  --process ez2dj3rd --seconds 150 demo=0x00a2946c autoplay=0x00a29508
    python guest_memory.py write --process ez2dj3rd 0x00a29508 1 --yes

The guest runs inside a re2dj host process at its own addresses on both OSes,
so --process matches a case-sensitive substring of an argument on the process's
command line: a direct run carries its profile ID (ez2dj3rd), a launcher's
child run its executable (EZ2DJ6TH.EXE). Of the matches the newest wins: on
Windows re2dj.exe starts itself again with the same command line and the guest
lives in that second process, and a launcher's child starts after the launcher.
Memory is reached through ReadProcessMemory on Windows and /proc/<pid>/mem on
Linux. Yama's ptrace_scope 1 lets only an ancestor read it, so --launch starts
the run from this script:

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
    PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
    TH32CS_SNAPPROCESS = 0x00000002
    INVALID_HANDLE_VALUE = wt.HANDLE(-1).value
    PROCESS_COMMAND_LINE_INFORMATION = 60

    class PROCESSENTRY32W(ctypes.Structure):
        _fields_ = [("dwSize", wt.DWORD), ("cntUsage", wt.DWORD), ("th32ProcessID", wt.DWORD),
                    ("th32DefaultHeapID", ctypes.c_size_t), ("th32ModuleID", wt.DWORD),
                    ("cntThreads", wt.DWORD), ("th32ParentProcessID", wt.DWORD),
                    ("pcPriClassBase", ctypes.c_long), ("dwFlags", wt.DWORD),
                    ("szExeFile", wt.WCHAR * 260)]

    class UNICODE_STRING(ctypes.Structure):
        _fields_ = [("Length", wt.USHORT), ("MaximumLength", wt.USHORT), ("Buffer", ctypes.c_void_p)]

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    ntdll = ctypes.WinDLL("ntdll")
    shell32 = ctypes.WinDLL("shell32", use_last_error=True)
    kernel32.OpenProcess.restype = wt.HANDLE
    kernel32.OpenProcess.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
    kernel32.CloseHandle.argtypes = [wt.HANDLE]
    kernel32.CreateToolhelp32Snapshot.restype = wt.HANDLE
    kernel32.CreateToolhelp32Snapshot.argtypes = [wt.DWORD, wt.DWORD]
    kernel32.Process32FirstW.argtypes = [wt.HANDLE, ctypes.POINTER(PROCESSENTRY32W)]
    kernel32.Process32NextW.argtypes = [wt.HANDLE, ctypes.POINTER(PROCESSENTRY32W)]
    kernel32.GetProcessTimes.argtypes = [wt.HANDLE] + [ctypes.POINTER(wt.FILETIME)] * 4
    kernel32.LocalFree.argtypes = [ctypes.c_void_p]
    ntdll.NtQueryInformationProcess.restype = ctypes.c_long
    ntdll.NtQueryInformationProcess.argtypes = [wt.HANDLE, ctypes.c_int, ctypes.c_void_p, wt.ULONG,
                                                ctypes.POINTER(wt.ULONG)]
    shell32.CommandLineToArgvW.restype = ctypes.POINTER(ctypes.c_wchar_p)
    shell32.CommandLineToArgvW.argtypes = [wt.LPCWSTR, ctypes.POINTER(ctypes.c_int)]
    kernel32.ReadProcessMemory.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t,
                                           ctypes.POINTER(ctypes.c_size_t)]
    kernel32.WriteProcessMemory.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t,
                                            ctypes.POINTER(ctypes.c_size_t)]


def parse_int(text):
    return int(text, 16) if text.lower().startswith("0x") else int(text, 10)


def windows_parents():
    """Maps every running pid to its parent pid."""
    snapshot = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0)
    if snapshot == INVALID_HANDLE_VALUE:
        return {}
    parents = {}
    try:
        entry = PROCESSENTRY32W()
        entry.dwSize = ctypes.sizeof(entry)
        more = kernel32.Process32FirstW(snapshot, ctypes.byref(entry))
        while more:
            parents[entry.th32ProcessID] = entry.th32ParentProcessID
            more = kernel32.Process32NextW(snapshot, ctypes.byref(entry))
    finally:
        kernel32.CloseHandle(snapshot)
    return parents


def windows_arguments_and_start(pid):
    """Returns (arguments, creation time) of a process, or None when it cannot be queried."""
    handle = kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
    if not handle:
        return None
    try:
        size = wt.ULONG(0)
        ntdll.NtQueryInformationProcess(handle, PROCESS_COMMAND_LINE_INFORMATION, None, 0, ctypes.byref(size))
        if size.value < ctypes.sizeof(UNICODE_STRING):
            return None
        buffer = ctypes.create_string_buffer(size.value)
        if ntdll.NtQueryInformationProcess(handle, PROCESS_COMMAND_LINE_INFORMATION, buffer, size,
                                           ctypes.byref(size)) != 0:
            return None
        text = UNICODE_STRING.from_buffer(buffer)
        command_line = ctypes.wstring_at(text.Buffer, text.Length // 2) if text.Buffer else ""
        times = [wt.FILETIME() for _ in range(4)]
        if not kernel32.GetProcessTimes(handle, *[ctypes.byref(t) for t in times]):
            return None
        created = (times[0].dwHighDateTime << 32) | times[0].dwLowDateTime
    finally:
        kernel32.CloseHandle(handle)
    count = ctypes.c_int(0)
    argv = shell32.CommandLineToArgvW(command_line, ctypes.byref(count))
    if not argv:
        return None
    try:
        arguments = [argv[i] for i in range(count.value)]
    finally:
        kernel32.LocalFree(argv)
    return arguments, created


def find_pid_windows(pattern, root):
    # Linux's rule, the newest by creation time as Windows pids are not
    # monotonic: re2dj.exe starts itself again with the same command line
    # and the guest lives in that second process.
    parents = windows_parents()
    excluded = ancestors_of(os.getpid(), parents) | {os.getpid()}
    found = None
    for pid in parents:
        if pid == 0 or pid in excluded:
            continue
        if root is not None and pid != root and root not in ancestors_of(pid, parents):
            continue
        queried = windows_arguments_and_start(pid)
        if queried is None:
            continue
        arguments, created = queried
        if any(pattern in argument for argument in arguments[1:]):
            if found is None or created > found[1]:
                found = (pid, created)
    return found[0] if found is not None else None


def parent_of(pid):
    try:
        with open("/proc/%d/status" % pid) as handle:
            for line in handle:
                if line.startswith("PPid:"):
                    return int(line.split()[1])
    except OSError:
        pass
    return 0


def ancestors_of(pid, parents=None):
    # Windows hands in its pid-to-parent map, where a reused pid can make a
    # chain loop, so a pid seen twice ends it.
    result = set()
    while pid > 1:
        pid = parents.get(pid, 0) if parents is not None else parent_of(pid)
        if pid in result:
            break
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
        pid = find_pid_windows(name, root) if WINDOWS else find_pid_linux(name, root)
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
                        help="a case-sensitive substring of a command-line argument of the re2dj run: "
                             "its profile ID, or a launcher child's executable")
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

    launched = None
    if args.launch:
        # Windows takes the command string as is; POSIX splitting would eat
        # the backslashes of its paths.
        launched = subprocess.Popen(args.launch if WINDOWS else shlex.split(args.launch))
    try:
        run(args, launched.pid if launched is not None else None)
    finally:
        if launched is not None:
            stop_tree(launched)


def stop_tree(launched):
    if WINDOWS:
        # The relaunched re2dj.exe and a launcher's child runs all descend
        # from the launched process.
        if launched.poll() is None:
            subprocess.run(["taskkill", "/T", "/F", "/PID", str(launched.pid)], capture_output=True)
        launched.wait()
        return
    # A launcher waits on its child, so the child goes first.
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
