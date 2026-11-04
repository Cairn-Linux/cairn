#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""The lock screen's fingerprint check must not spend a Guardian's try on a
computer with no reader (#283, ADR-0051).

Runs the image's real kde-fingerprint stack through libpam, from a copy of
/etc/pam.d in /tmp, for the account nobody. The build has no fprintd, so
pam_fprintd answers as it does with no reader. The faillock lines write to
/tmp, so nothing in the image changes. Fedora's own line runs first, to show
the check can see a record at all.
"""

import ctypes
import pathlib
import shutil
import subprocess
import sys
import tempfile

PAM_SUCCESS = 0
AUTHINFO_UNAVAIL = 9
OURS = "[success=done authinfo_unavail=die default=bad]"
FEDORAS = "[success=done default=bad]"
# Fedora's plasma-workspace kde-fingerprint, which ours copies. A new one
# needs a look before this changes.
PACKAGED = "8b3181ce5979f498e2cd07acaf3f57c63b1e2f9593e44bfa9027b6dd8bb62437"
PAM_FOLDERS = [pathlib.Path("/etc/pam.d"), pathlib.Path("/usr/lib/pam.d")]

pam = ctypes.CDLL("libpam.so.0")


class Conversation(ctypes.Structure):
    _fields_ = [("conv", ctypes.c_void_p), ("appdata", ctypes.c_void_p)]


def authenticate(confdir, service, user):
    """PAM's answer, or None if PAM would not start. With no conversation,
    a module that asks for anything fails, which this check counts as a
    failure too."""
    handle = ctypes.c_void_p()
    conversation = Conversation(None, None)
    started = pam.pam_start_confdir(service.encode(), user.encode(), ctypes.byref(conversation),
                                    str(confdir).encode(), ctypes.byref(handle))
    if started != PAM_SUCCESS:
        return None
    result = pam.pam_authenticate(handle, 0)
    pam.pam_end(handle, result)
    return result


def records(folder, user):
    out = subprocess.run(["faillock", "--dir", str(folder), "--user", user],
                         capture_output=True, text=True, check=True).stdout
    return sum(1 for line in out.splitlines() if line[:2] == "20")


def run(stack_text, line_control):
    with tempfile.TemporaryDirectory() as work:
        work = pathlib.Path(work)
        confdir, tally = work / "pam.d", work / "faillock"
        shutil.copytree("/etc/pam.d", confdir, symlinks=False)
        tally.mkdir()
        text = stack_text.replace(OURS, line_control)
        text = text.replace("pam_faillock.so preauth silent", f"pam_faillock.so preauth silent dir={tally}")
        text = text.replace("pam_faillock.so authfail", f"pam_faillock.so authfail dir={tally}")
        (confdir / "kde-fingerprint").write_text(text)
        return authenticate(confdir, "kde-fingerprint", "nobody"), records(tally, "nobody")


def main():
    failures = []
    dump = subprocess.run(["rpm", "-q", "--dump", "plasma-workspace"],
                          capture_output=True, text=True, check=True).stdout
    digests = [line.split()[3] for line in dump.splitlines() if line.startswith("/etc/pam.d/kde-fingerprint ")]
    if digests != [PACKAGED]:
        failures.append("plasma-workspace has a new kde-fingerprint: compare it with session/pam.d/kde-fingerprint")
    template = pathlib.Path("/usr/share/authselect/default/local/fingerprint-auth").read_text()
    if FEDORAS not in template:
        failures.append("authselect's fingerprint-auth has a new pam_fprintd line: compare it with ours")
    for folder in PAM_FOLDERS:
        if not folder.is_dir():
            continue
        for path in folder.iterdir():
            for line in path.read_text().splitlines():
                words = line.split()
                if len(words) >= 3 and words[0].lstrip("-") == "auth" and words[2] == "fingerprint-auth":
                    failures.append(f"{path} runs fingerprint-auth's auth lines, which count a missing reader")
    stack = pathlib.Path("/etc/pam.d/kde-fingerprint").read_text()
    if OURS not in stack:
        failures.append("the image's kde-fingerprint is not Cairn's")
    else:
        if run(stack, FEDORAS) != (AUTHINFO_UNAVAIL, 1):
            failures.append("Fedora's line no longer writes a record here, so this check proves nothing")
        result = run(stack, OURS)
        if result != (AUTHINFO_UNAVAIL, 0):
            failures.append(f"with no reader the lock screen's check gave {result[0]} and {result[1]} records")
    for failure in failures:
        print(failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
