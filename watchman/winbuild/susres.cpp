/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <errno.h>
#include <fmt/core.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <tlhelp32.h>

// Super simple utility to suspend or resume all threads in a target process.
// We use this in place of `kill -STOP` and `kill -CONT`

typedef LONG(NTAPI* sus_res_func)(HANDLE proc);

const char* win32_strerror(DWORD err) {
  static char msgbuf[1024];
  FormatMessageA(
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr,
      err,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      msgbuf,
      sizeof(msgbuf) - 1,
      nullptr);
  return msgbuf;
}

int apply(DWORD pid, BOOL suspend) {
  sus_res_func func;
  const char* name = suspend ? "NtSuspendProcess" : "NtResumeProcess";
  HANDLE proc;
  DWORD res;

  func = (sus_res_func)GetProcAddress(GetModuleHandle("ntdll"), name);

  if (!func) {
    fmt::print(
        "Failed to GetProcAddress({}): {}\n",
        name,
        win32_strerror(GetLastError()));
    return 1;
  }

  proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
  if (proc == INVALID_HANDLE_VALUE) {
    fmt::print(
        "Failed to OpenProcess({}): {}\n", pid, win32_strerror(GetLastError()));
    return 1;
  }

  res = func(proc);

  if (res) {
    fmt::print(
        "{}({}) returns {:x}: {}\n", name, pid, res, win32_strerror(res));
  }

  CloseHandle(proc);

  return res == 0 ? 0 : 1;
}

int status(DWORD pid) {
  HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (snapshot == INVALID_HANDLE_VALUE) {
    fmt::print(
        "Failed to CreateToolhelp32Snapshot: {}\n",
        win32_strerror(GetLastError()));
    return 1;
  }

  THREADENTRY32 entry;
  entry.dwSize = sizeof(entry);

  BOOL haveThread = Thread32First(snapshot, &entry);
  bool foundThread = false;
  bool allSuspended = true;

  while (haveThread) {
    if (entry.th32OwnerProcessID == pid) {
      foundThread = true;
      HANDLE thread =
          OpenThread(THREAD_SUSPEND_RESUME, FALSE, entry.th32ThreadID);
      if (!thread) {
        fmt::print(
            "Failed to OpenThread({}): {}\n",
            entry.th32ThreadID,
            win32_strerror(GetLastError()));
        CloseHandle(snapshot);
        return 1;
      }

      DWORD previousCount = SuspendThread(thread);
      if (previousCount == static_cast<DWORD>(-1)) {
        fmt::print(
            "SuspendThread({}) failed: {}\n",
            entry.th32ThreadID,
            win32_strerror(GetLastError()));
        CloseHandle(thread);
        CloseHandle(snapshot);
        return 1;
      }

      DWORD resumeCount = ResumeThread(thread);
      if (resumeCount == static_cast<DWORD>(-1)) {
        fmt::print(
            "ResumeThread({}) failed: {}\n",
            entry.th32ThreadID,
            win32_strerror(GetLastError()));
        CloseHandle(thread);
        CloseHandle(snapshot);
        return 1;
      }

      if (previousCount == 0) {
        allSuspended = false;
        CloseHandle(thread);
        break;
      }

      CloseHandle(thread);
    }

    haveThread = Thread32Next(snapshot, &entry);
  }

  CloseHandle(snapshot);

  if (!foundThread) {
    fmt::print("No threads found for pid {}\n", pid);
    return 1;
  }

  fmt::print("{}\n", allSuspended ? "T" : "R");
  return 0;
}

void usage() {
  fmt::print(
      "Usage: susres suspend [pid]\n"
      "       susres resume  [pid]\n"
      "       susres status  [pid]\n");
  exit(1);
}

enum class Command {
  Suspend,
  Resume,
  Status,
};

int main(int argc, char** argv) {
  DWORD pid;
  Command cmd;

  if (argc != 3) {
    usage();
  }

  if (!strcmp(argv[1], "suspend")) {
    cmd = Command::Suspend;
  } else if (!strcmp(argv[1], "resume")) {
    cmd = Command::Resume;
  } else if (!strcmp(argv[1], "status")) {
    cmd = Command::Status;
  } else {
    usage();
  }

  pid = _atoi64(argv[2]);

  switch (cmd) {
    case Command::Suspend:
      return apply(pid, TRUE);
    case Command::Resume:
      return apply(pid, FALSE);
    case Command::Status:
      return status(pid);
  }

  return 1;
}

/* vim:ts=2:sw=2:et:
 */
