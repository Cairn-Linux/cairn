#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Prove faillock tallies resist a distinct unprivileged attacker (#301).

The image's tmpfiles rules make the setgid directory and its default ACL. The
shared reconciliation helper repairs a migrated wrong-owner tally and replaces
its access ACL. The real pam_faillock then exercises new counts, successful
authentication, and administrative reset independently.
"""

import ctypes
import os
import pathlib
import pwd
import subprocess
import sys
import tempfile

PAM_SUCCESS = 0
TMPFILES = pathlib.Path("/usr/lib/tmpfiles.d/cairn.conf")
RECONCILER = pathlib.Path("/usr/libexec/cairn/cairn-reconcile-faillock")
FOLDER = "var/lib/faillock"
USER = "nobody"
ATTACKER = "daemon"

pam = ctypes.CDLL("libpam.so.0")


class Conversation(ctypes.Structure):
    _fields_ = [("conv", ctypes.c_void_p), ("appdata", ctypes.c_void_p)]


def run_pam(confdir, line, caller_group):
    """Run one pam_faillock line for USER with su's caller-group shape."""
    (confdir / "cairn-check").write_text(f"auth required pam_faillock.so {line}\n")
    handle = ctypes.c_void_p()
    conversation = Conversation(None, None)
    started = pam.pam_start_confdir(
        b"cairn-check",
        USER.encode(),
        ctypes.byref(conversation),
        str(confdir).encode(),
        ctypes.byref(handle),
    )
    if started != PAM_SUCCESS:
        return None
    os.setegid(caller_group)
    try:
        result = pam.pam_authenticate(handle, 0)
    finally:
        os.setegid(0)
    pam.pam_end(handle, result)
    return result


def owner(path):
    """Return user:group:mode, as stat --format does."""
    return subprocess.run(
        ["stat", "--format=%U:%G:%a", "--", str(path)],
        capture_output=True,
        text=True,
        check=True,
    ).stdout.strip()


def acl(path):
    return subprocess.run(
        ["getfacl", "--omit-header", "--absolute-names", "--", str(path)],
        capture_output=True,
        text=True,
        check=True,
    ).stdout.splitlines()


def attacker_truncate_result(path, attacker):
    """Return 0 for denied, 1 for opened, or 2 for another OS failure."""
    pid = os.fork()
    if pid == 0:
        try:
            os.setgroups([])
            os.setgid(attacker.pw_gid)
            os.setuid(attacker.pw_uid)
            descriptor = os.open(path, os.O_WRONLY | os.O_TRUNC)
            os.close(descriptor)
        except PermissionError:
            os._exit(0)
        except OSError:
            os._exit(2)
        os._exit(1)
    _, status = os.waitpid(pid, 0)
    return os.waitstatus_to_exitcode(status)


def check_tally(path, failures, label):
    expected = f"{USER}:root:660"
    if not path.exists() or owner(path) != expected:
        failures.append(f"{label} is {owner(path) if path.exists() else 'missing'}, not {expected}")


def main():
    failures = []
    account = pwd.getpwnam(USER)
    attacker = pwd.getpwnam(ATTACKER)
    if account.pw_uid == attacker.pw_uid:
        print("target and attacker identities are not distinct", file=sys.stderr)
        return 1

    with tempfile.TemporaryDirectory() as temporary:
        work = pathlib.Path(temporary)
        os.chmod(work, 0o755)
        root, confdir, plain = work / "root", work / "pam.d", work / "plain"
        confdir.mkdir()
        (root / "etc").mkdir(parents=True)
        with open(root / "etc/passwd", "w") as passwd, open(root / "etc/group", "w") as group:
            for name in ("/etc/passwd", "/usr/lib/passwd"):
                if os.path.exists(name):
                    passwd.write(pathlib.Path(name).read_text())
            for name in ("/etc/group", "/usr/lib/group"):
                if os.path.exists(name):
                    group.write(pathlib.Path(name).read_text())

        # Negative control: without setgid, pam_faillock keeps the caller group.
        plain.mkdir(mode=0o755)
        made = run_pam(confdir, f"authfail dir={plain}", attacker.pw_gid)
        if made is None or not (plain / USER).exists():
            print(
                f"pam_faillock made no count in a plain folder (PAM answered {made}), so this check cannot run",
                file=sys.stderr,
            )
            return 1
        if os.stat(plain / USER).st_gid != attacker.pw_gid:
            failures.append("the negative control did not take the attacker's group")

        faillock = root / FOLDER
        faillock.mkdir(parents=True)
        lines = [
            line
            for line in TMPFILES.read_text().splitlines()
            if line[:1] != "#" and line.split()[1:2] == [f"/{FOLDER}"]
        ]
        config = work / "faillock.conf"
        config.write_text("\n".join(lines) + "\n")
        subprocess.run(
            ["systemd-tmpfiles", "--create", f"--root={root}", str(config)],
            check=True,
        )
        if owner(faillock) != "root:root:2755":
            failures.append(f"tmpfiles leaves {FOLDER} {owner(faillock)}, not root:root:2755")

        # Migrated state: Guardian-named, child-owned, and explicitly writable
        # by that child. The helper must replace every part of this policy.
        tally = faillock / USER
        tally.write_bytes(b"migrated count")
        os.chown(tally, attacker.pw_uid, attacker.pw_gid)
        os.chmod(tally, 0o660)
        subprocess.run(["setfacl", "-m", f"u:{ATTACKER}:rw", "--", str(tally)], check=True)
        subprocess.run([str(RECONCILER), str(faillock)], check=True)
        check_tally(tally, failures, "the repaired migrated tally")
        repaired_acl = acl(tally)
        if "user:sddm:r--" not in repaired_acl or f"user:{ATTACKER}:rw-" in repaired_acl:
            failures.append(f"the repaired tally has the wrong access ACL: {repaired_acl}")
        truncate_result = attacker_truncate_result(tally, attacker)
        if truncate_result == 1:
            failures.append("the distinct unprivileged attacker could truncate the repaired tally")
        elif truncate_result != 0:
            failures.append("the distinct unprivileged write-denial check failed unexpectedly")

        # New count through the image's real PAM path.
        tally.unlink()
        run_pam(confdir, f"authfail dir={faillock}", attacker.pw_gid)
        check_tally(tally, failures, "a new tally")
        if not tally.exists() or tally.stat().st_size == 0:
            failures.append("pam_faillock made no non-empty tally for authsucc")
        else:
            run_pam(confdir, f"authsucc dir={faillock}", attacker.pw_gid)
            check_tally(tally, failures, "the tally after authsucc")
            if tally.stat().st_size != 0:
                failures.append("successful authentication did not clear the tally")

        # Administrative reset gets a fresh non-empty record so authsucc cannot
        # mask a broken reset.
        run_pam(confdir, f"authfail dir={faillock}", attacker.pw_gid)
        if not tally.exists() or tally.stat().st_size == 0:
            failures.append("pam_faillock made no non-empty tally for reset")
        else:
            subprocess.run(
                ["faillock", "--dir", str(faillock), "--user", USER, "--reset"],
                check=True,
            )
            check_tally(tally, failures, "the tally after administrative reset")
            if tally.stat().st_size != 0:
                failures.append("administrative reset did not clear the tally")

    for failure in failures:
        print(failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
