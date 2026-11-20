"""Run one Windows stress probe in a memory-limited, low-priority Windows job."""
import argparse
from contextlib import ExitStack
import ctypes as c
from ctypes import wintypes as w
import json
import msvcrt
import os
from pathlib import Path
import subprocess
import sys
import time


kernel = c.WinDLL("kernel32", use_last_error=True)
P, H, D, SZ, U64 = c.c_void_p, w.HANDLE, w.DWORD, c.c_size_t, c.c_uint64
MiB = 1024 * 1024


class MemoryStatus(c.Structure):
    _fields_ = [("length", D), ("load", D)] + [
        (key, U64) for key in "physical avail_phys commit avail_commit virtual avail_virt extra".split()]


class BasicLimits(c.Structure):
    _fields_ = [("process_time", U64), ("job_time", U64), ("flags", D), ("min_ws", SZ),
                ("max_ws", SZ), ("processes", D), ("affinity", SZ), ("priority", D), ("sched", D)]


class ExtendedLimits(c.Structure):
    _fields_ = [("basic", BasicLimits), ("io", U64 * 6), ("process_limit", SZ),
                ("job_limit", SZ), ("peak_process", SZ), ("peak_job", SZ)]


class MemoryCounters(c.Structure):
    _fields_ = [("length", D), ("faults", D)] + [
        (key, SZ) for key in "peak_ws ws peak_paged paged peak_nonpaged nonpaged commit peak_commit".split()]


class Startup(c.Structure):
    _fields_ = [("size", D), ("reserved", P), ("desktop", P), ("title", P)] + [
        (key, D) for key in "x y cx cy chars_x chars_y fill flags".split()] + [
        ("show", w.WORD), ("reserved_size", w.WORD), ("reserved_ptr", P),
        ("stdin", H), ("stdout", H), ("stderr", H), ("attribute_list", P)]


class ProcessInfo(c.Structure):
    _fields_ = [("process", H), ("thread", H), ("pid", D), ("tid", D)]


def api(name, result, *args):
    fn = getattr(kernel, name)
    fn.argtypes, fn.restype = args, result
    return fn


def check(value):
    if not value:
        raise c.WinError(c.get_last_error())
    return value


close = api("CloseHandle", w.BOOL, H)
global_memory = api("GlobalMemoryStatusEx", w.BOOL, P)
create_job = api("CreateJobObjectW", H, P, w.LPCWSTR)
set_job_info = api("SetInformationJobObject", w.BOOL, H, D, P, D)
query_job_info = api("QueryInformationJobObject", w.BOOL, H, D, P, D, P)
create_port = api("CreateIoCompletionPort", H, H, H, SZ, D)
poll_port = api("GetQueuedCompletionStatus", w.BOOL, H, P, P, P, D)
init_attrs = api("InitializeProcThreadAttributeList", w.BOOL, P, D, D, P)
set_attr = api("UpdateProcThreadAttribute", w.BOOL, P, D, SZ, P, SZ, P, P)
delete_attrs = api("DeleteProcThreadAttributeList", None, P)
create_proc = api("CreateProcessW", w.BOOL, w.LPCWSTR, w.LPWSTR, P, P, w.BOOL,
                  D, P, w.LPCWSTR, P, P)
wait_proc = api("WaitForSingleObject", D, H, D)
kill_job = api("TerminateJobObject", w.BOOL, H, w.UINT)
exit_code = api("GetExitCodeProcess", w.BOOL, H, P)
memory_info = api("K32GetProcessMemoryInfo", w.BOOL, H, P, D)


def host_available():
    status = MemoryStatus()
    status.length = c.sizeof(status)
    check(global_memory(c.byref(status)))
    return status.avail_phys, status.avail_commit


def launch(stack, job, args):
    # Put the probe in its job atomically. Killing the supervisor cannot leave
    # even a suspended probe outside the job. Only STDIO handles are inherited.
    devnull = stack.enter_context(open(os.devnull, "rb"))
    streams = [devnull, sys.stdout, sys.stderr]
    fds = [os.dup(stream.fileno()) for stream in streams]
    for fd in fds:
        stack.callback(os.close, fd)
        os.set_inheritable(fd, True)
    streams_array = (H * 3)(*(msvcrt.get_osfhandle(fd) for fd in fds))
    jobs_array = (H * 1)(job)
    required = SZ()
    init_attrs(None, 2, 0, c.byref(required))
    buffer = c.create_string_buffer(required.value)
    check(init_attrs(buffer, 2, 0, c.byref(required)))
    try:
        for key, values in [(0x20002, streams_array), (0x2000D, jobs_array)]:
            check(set_attr(buffer, 0, key, values, c.sizeof(values), None, None))
        si = Startup()
        si.size, si.flags = c.sizeof(si), 0x100  # STARTF_USESTDHANDLES
        si.stdin, si.stdout, si.stderr = streams_array
        si.attribute_list = c.addressof(buffer)
        process = ProcessInfo()
        cmdline = c.create_unicode_buffer(subprocess.list2cmdline(args.command))
        # EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS
        check(create_proc(args.command[0], cmdline, None, None, True, 0x08090000,
                          None, args.cwd, c.byref(si), c.byref(process)))
    finally:
        delete_attrs(buffer)
    close(process.thread)
    stack.callback(close, process.process)
    return process.process


def limit_event(port):
    hit = False
    event, key, pointer = D(), SZ(), P()
    while poll_port(port, c.byref(event), c.byref(key), c.byref(pointer), 0):
        hit |= event.value in (9, 10)  # process/job memory denied
    return hit


def supervise(args, report):
    free, commit = host_available()
    reserve = args.reserve_physical_mib * MiB
    commit_reserve = args.reserve_commit_mib * MiB
    budget = min(args.max_private_mib * MiB, free - reserve, commit - commit_reserve)
    report.update(initialAvailablePhysicalBytes=free, initialAvailableCommitBytes=commit,
                  reservedPhysicalBytes=reserve, reservedCommitBytes=commit_reserve,
                  appliedCommitLimitBytes=max(budget, 0), timeoutSeconds=args.timeout_sec)
    if budget < 64 * MiB:
        report["status"] = "insufficient_host_headroom"
        return 3
    with ExitStack() as stack:
        job = check(create_job(None, None))
        stack.callback(close, job)
        limits = ExtendedLimits()
        # KILL_ON_JOB_CLOSE | JOB_MEMORY | PROCESS_MEMORY | ACTIVE_PROCESS
        limits.basic.flags, limits.basic.processes = 0x2308, 1
        limits.job_limit = limits.process_limit = budget
        check(set_job_info(job, 9, c.byref(limits), c.sizeof(limits)))
        port = check(create_port(H(-1), None, 0, 1))
        stack.callback(close, port)
        assoc = (P * 2)(1, port)
        check(set_job_info(job, 7, assoc, c.sizeof(assoc)))
        process = launch(stack, job, args)
        started = time.monotonic()
        peak_ws = 0
        why = None
        while True:
            wait = wait_proc(process, 200)
            if wait not in (0, 258):
                raise c.WinError(c.get_last_error())
            counters = MemoryCounters()
            counters.length = c.sizeof(counters)
            if memory_info(process, c.byref(counters), counters.length):
                peak_ws = max(peak_ws, counters.peak_ws)
            if limit_event(port):
                why = "job_memory_limit"
            if not wait:
                break
            free, commit = host_available()
            if why is None:
                if free < reserve:
                    why = "host_physical_reserve"
                elif commit < commit_reserve:
                    why = "host_commit_reserve"
                elif time.monotonic() - started >= args.timeout_sec:
                    why = "timeout"
            if why:
                check(kill_job(job, 222))
                check(wait_proc(process, 10000) == 0)
                break
        child_exit = D()
        check(exit_code(process, c.byref(child_exit)))
        check(query_job_info(job, 9, c.byref(limits), c.sizeof(limits), None))
        report.update(status=why or ("passed" if child_exit.value == 0 else "child_failed"),
                      exitCode=child_exit.value, elapsedSeconds=round(time.monotonic()-started, 3),
                      peakPrivateCommitBytes=limits.peak_process, peakWorkingSetBytes=peak_ws)
        return 0 if report["status"] == "passed" else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cwd", type=str, default=os.getcwd())
    parser.add_argument("--max-private-mib", type=int, default=4096)
    parser.add_argument("--reserve-physical-mib", type=int, default=1536)
    parser.add_argument("--reserve-commit-mib", type=int, default=2048)
    parser.add_argument("--timeout-sec", type=float, default=600)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.command[:1] == ["--"]:
        args.command.pop(0)
    if not args.command or not Path(args.command[0]).is_absolute():
        parser.error("supply -- followed by the absolute EXE path and its arguments")
    if min(args.max_private_mib, args.reserve_physical_mib, args.reserve_commit_mib, args.timeout_sec) <= 0:
        parser.error("memory and time limits must be positive")
    report = {"record": "guard"}
    try:
        result = supervise(args, report)
    except (OSError, RuntimeError) as exc:
        report.update(status="guard_error", error=str(exc))
        result = 3
    print(json.dumps(report, ensure_ascii=True), flush=True)
    return result


if __name__ == "__main__":
    sys.exit(main())
