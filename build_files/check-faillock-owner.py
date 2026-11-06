#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""A child with su must never be able to write another account's faillock
count (#289, ADR-0053).

Runs the image's own tmpfiles lines for /var/lib/faillock into a copy of
the folder in /tmp, then the image's real pam_faillock through libpam, as
su runs it: as root, with the effective group of whoever typed su. The
account nobody, and nobody's group, stand in for both accounts. Nothing in
the image changes. A run in a folder without setgid comes first, to show
the check can see the caller's group at all.
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
FOLDER = "var/lib/faillock"
USER = "nobody"

pam = ctypes.CDLL("libpam.so.0")


class Conversation(ctypes.Structure):
    _fields_ = [("conv", ctypes.c_void_p), ("appdata", ctypes.c_void_p)]


def run_pam(confdir, line, caller_group):
    """Runs a one-line auth stack for USER with `caller_group` as the
    effective group, as su does. PAM's answer, or None if it would not
    start."""
    (confdir / "cairn-check").write_text(f"auth required pam_faillock.so {line}\n")
    handle = ctypes.c_void_p()
    conversation = Conversation(None, None)
    started = pam.pam_start_confdir(b"cairn-check", USER.encode(), ctypes.byref(conversation),
                                    str(confdir).encode(), ctypes.byref(handle))
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
    """user:group:mode, as stat --format=%U:%G:%a gives it."""
    return subprocess.run(["stat", "--format=%U:%G:%a", "--", str(path)],
                          capture_output=True, text=True, check=True).stdout.strip()


def main():
    failures = []
    account = pwd.getpwnam(USER)
    with tempfile.TemporaryDirectory() as work:
        work = pathlib.Path(work)
        root, confdir, plain = work / "root", work / "pam.d", work / "plain"
        confdir.mkdir()
        # The image's accounts, for the ACL line that names sddm: tmpfiles
        # with --root reads only the root's /etc. Only the folder's own
        # lines, since the groups Cairn adds come at first boot.
        (root / "etc").mkdir(parents=True)
        with open(root / "etc/passwd", "w") as passwd, open(root / "etc/group", "w") as group:
            for name in ("/etc/passwd", "/usr/lib/passwd"):
                if os.path.exists(name):
                    passwd.write(pathlib.Path(name).read_text())
            for name in ("/etc/group", "/usr/lib/group"):
                if os.path.exists(name):
                    group.write(pathlib.Path(name).read_text())

        # Without setgid, a count su makes takes the caller's group.
        plain.mkdir(mode=0o755)
        made = run_pam(confdir, f"authfail dir={plain}", account.pw_gid)
        if made is None or not (plain / USER).exists():
            print(f"pam_faillock made no count in a plain folder (PAM answered {made}), "
                  "so this check cannot run", file=sys.stderr)
            return 1
        if os.stat(plain / USER).st_gid != account.pw_gid:
            failures.append("a count made without setgid did not take the caller's group, "
                            "so this check proves nothing")

        # A count su made before this change, with the caller's group.
        faillock = root / FOLDER
        faillock.mkdir(parents=True)
        old = faillock / USER
        old.write_bytes(b"")
        os.chown(old, account.pw_uid, account.pw_gid)
        os.chmod(old, 0o660)
        lines = [line for line in TMPFILES.read_text().splitlines()
                 if line[:1] != "#" and line.split()[1:2] in ([f"/{FOLDER}"], [f"/{FOLDER}/*"])]
        (work / "faillock.conf").write_text("\n".join(lines) + "\n")
        subprocess.run(["systemd-tmpfiles", "--create", f"--root={root}", str(work / "faillock.conf")],
                       check=True)
        if owner(faillock) != "root:root:2755":
            failures.append(f"tmpfiles leaves {FOLDER} {owner(faillock)}, not root:root:2755")
        if owner(old) != f"{USER}:root:660":
            failures.append(f"tmpfiles leaves a count with the caller's group {owner(old)}, "
                            f"not {USER}:root:660")

        # A count su makes from now on.
        old.unlink()
        run_pam(confdir, f"authfail dir={faillock}", account.pw_gid)
        if not old.exists() or owner(old) != f"{USER}:root:660":
            failures.append("a count made by su in the image's folder is "
                            f"{owner(old) if old.exists() else 'missing'}, not {USER}:root:660")
        else:
            acl = subprocess.run(["getfacl", "--omit-header", "--absolute-names", "--", str(old)],
                                 capture_output=True, text=True, check=True).stdout.splitlines()
            if "user:sddm:r--" not in acl or "group::rw-" not in acl:
                failures.append(f"a new count lost the login screen's read entry: {acl}")
            # A good login and faillock --reset empty it and keep the group.
            run_pam(confdir, f"authsucc dir={faillock}", account.pw_gid)
            subprocess.run(["faillock", "--dir", str(faillock), "--user", USER, "--reset"], check=True)
            if owner(old) != f"{USER}:root:660" or old.stat().st_size != 0:
                failures.append(f"after a good login and a reset the count is {owner(old)}, "
                                f"{old.stat().st_size} bytes")
    for failure in failures:
        print(failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
