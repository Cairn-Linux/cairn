#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Runs bin/cairn-steam-visit with stand-ins on PATH (ADR-0032).

Standard library only. The helper is sourced, its paths are pointed into a
temporary directory, and `main` runs as the test's own user. Stand-ins for
`id`, `getent`, `loginctl`, `pgrep` and the rest answer from the scenario
and write what they were asked to a log; nothing on this machine is
started or stopped. The real runs are in the VM
(docs/research/steam-visit.md).
"""

import os
import pathlib
import signal
import socket
import subprocess
import tempfile
import unittest

HELPER = pathlib.Path(__file__).resolve().parent.parent / "bin" / "cairn-steam-visit"
PROVISION = pathlib.Path(__file__).resolve().parents[2] / "provision" / "cairn-provision.sh"
ME = os.getuid()

# Each stand-in reads the scenario from files in the state directory, so a
# test changes one file to change one answer.
STUBS = {
    "id": r"""#!/bin/bash
echo "id $*" >> "$STATE/calls"
case "$*" in
    -u) echo 0 ;;
    "-nu -- $GUARDIAN_UID") echo guardian ;;
    "-nu -- "*) exit 1 ;;
    "-nG -- guardian") cat "$STATE/guardian-groups" ;;
    "-nG -- "*) cat "$STATE/child-groups" ;;
    "-u -- "*) echo "$CHILD_UID" ;;
    "-g -- "*) echo "$CHILD_UID" ;;
esac
""",
    "getent": r"""#!/bin/bash
[ "$1 $2" = "passwd ada" ] || exit 2
echo "ada:x:$CHILD_UID:$CHILD_UID::$STATE/home/ada:/bin/bash"
""",
    "loginctl": r"""#!/bin/bash
echo "loginctl $*" >> "$STATE/calls"
case "$1" in
    show-user)
        case "$3" in
            --property=State) cat "$STATE/guardian-state" ;;
            --property=Sessions)
                # A login that gets past PAM once the visit is open.
                if [ -e "$STATE/login-during-visit" ] && grep -q '^systemd-run' "$STATE/calls"; then
                    echo c9
                else
                    cat "$STATE/child-sessions"
                fi ;;
        esac ;;
    show-session)
        case "$2:$3" in
            c9:--property=Class) echo user ;;
            c9:--property=Service) echo sddm ;;
            *:--property=Class) cat "$STATE/session-class" ;;
            *:--property=Service) cat "$STATE/session-service" ;;
        esac ;;
    terminate-user) touch "$STATE/terminated" ;;
esac
""",
    "pgrep": r"""#!/bin/bash
echo "pgrep $*" >> "$STATE/calls"
case "$*" in
    *"--exact steam") [ -e "$STATE/steam-running" ] && ! grep -q 'steam -shutdown' "$STATE/calls" ;;
    *"--exact cage-visit") [ -e "$STATE/cage-starts" ] && grep -q '^systemd-run' "$STATE/calls" ;;
    *)
        # Any program of the child's. Before the visit: a number of
        # leftovers from the last login, then none. During it: the visit's
        # own, until the child is ended, or with "needs-kill" until killed.
        if grep -q '^systemd-run' "$STATE/calls"; then
            [ -e "$STATE/visit-survives" ] && exit 0
            [ -e "$STATE/needs-kill" ] && ! grep -q '^pkill' "$STATE/calls" && exit 0
            [ -e "$STATE/terminated" ] && exit 1
            exit 0
        fi
        left=$(cat "$STATE/leftover" 2>/dev/null || echo 0)
        [ "$left" -gt 0 ] || exit 1
        echo $((left - 1)) > "$STATE/leftover"
        exit 0 ;;
esac
""",
    "systemd-run": r"""#!/bin/bash
echo "systemd-run $*" >> "$STATE/calls"
cat "$VISITS" > "$STATE/listed-during-visit"
""",
    "timeout": r"""#!/bin/bash
echo "timeout $*" >> "$STATE/calls"
""",
    "selinuxenabled": r"""#!/bin/bash
[ -e "$STATE/selinux" ]
""",
    # The copy program, run as the child: it says what the scenario says,
    # and notes what was listed while it ran.
    "setpriv": r"""#!/bin/bash
echo "setpriv $*" >> "$STATE/calls"
cat "$VISITS" > "$STATE/listed-during-copy" 2> /dev/null
cat "$COPYING" > "$STATE/marked-during-copy" 2> /dev/null
cat "$STATE/copy-says" 2> /dev/null
echo "a word on the error output" >&2
exit "$(cat "$STATE/copy-status" 2> /dev/null || echo 0)"
""",
}
for name in ("systemctl", "pkill", "restorecon", "sleep", "logger"):
    STUBS[name] = f'#!/bin/bash\necho "{name} $*" >> "$STATE/calls"\n'

RUN = r"""
source "$HELPER"
RUN_DIR="$STATE/run/cairn"
VISITS="$RUN_DIR/steam-visits"
COPYING="$RUN_DIR/steam-copy"
RUNTIME_ROOT="$STATE/run/user"
X11_DIR="$STATE/tmp"
STEAM_SECONDS=3
END_SECONDS=3
if [ -e "$STATE/stop-early" ]; then
    stopping=yes
fi
if [ -e "$STATE/labelled" ]; then
    label_of() { echo etc_t; }
fi
PATH="$STATE/bin:$PATH"
main "$@"
"""


class Scenario:
    """One run of the helper: the answers the system gives, and what it did."""

    def __init__(self, directory):
        self.state = pathlib.Path(directory)
        (self.state / "bin").mkdir()
        for name, body in STUBS.items():
            stub = self.state / "bin" / name
            stub.write_text(body)
            stub.chmod(0o755)
        (self.state / "run" / "cairn").mkdir(parents=True)
        (self.state / "tmp" / ".X11-unix").mkdir(parents=True)
        runtime = self.state / "run" / "user" / str(ME)
        runtime.mkdir(parents=True)
        self.wayland = runtime / "wayland-0"
        self.listener = socket.socket(socket.AF_UNIX)
        self.listener.bind(str(self.wayland))
        self.set("guardian-groups", "guardian wheel cairn-guardian")
        self.set("child-groups", "ada cairn-l1")
        self.set("guardian-state", "active")
        self.set("child-sessions", "")
        self.set("session-class", "manager")
        self.set("session-service", "systemd-user")
        self.flag("cage-starts")
        self.env = {
            "PATH": "/usr/bin:/bin",
            "HELPER": str(HELPER),
            "STATE": str(self.state),
            "VISITS": str(self.visits),
            "COPYING": str(self.marker),
            "GUARDIAN_UID": str(ME),
            "CHILD_UID": str(ME),
            "PKEXEC_UID": str(ME),
        }

    @property
    def visits(self):
        return self.state / "run" / "cairn" / "steam-visits"

    @property
    def marker(self):
        return self.state / "run" / "cairn" / "steam-copy"

    def set(self, name, text):
        (self.state / name).write_text(text + "\n" if text else "")

    def flag(self, name):
        (self.state / name).touch()

    def unflag(self, name):
        (self.state / name).unlink(missing_ok=True)

    def run(self, child="ada", stdin="done\n", keep_stdin_open=False, signal_after_open=None, copy=False,
            appids=()):
        """Run the helper once. With keep_stdin_open, nothing is written and
        stdin stays open until the helper ends by itself. With
        signal_after_open, that signal is sent once it says "open". With
        copy, it runs as provisioning's copy of the games, of `appids`
        ("--all" for every game). A child of None passes no child at all."""
        for name in ("calls", "terminated"):
            self.unflag(name)
        named = [] if child is None else [child]
        arguments = ["--copy-games", *named, *appids] if copy else named
        with open(self.state / "trace", "w") as trace:
            process = subprocess.Popen(
                ["bash", "-c", RUN, "cairn-steam-visit", *arguments],
                env=self.env,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=trace,
                text=True,
            )
            if signal_after_open:
                first = process.stdout.readline()
                process.send_signal(signal_after_open)
                process.wait(timeout=30)
                out = first + process.stdout.read()
                process.stdin.close()
                process.stdout.close()
            elif keep_stdin_open:
                process.wait(timeout=30)
                out = process.stdout.read()
                process.stdin.close()
                process.stdout.close()
            else:
                out, _ = process.communicate(stdin, timeout=30)
        self.code = process.returncode
        self.said = out.split("\n")[:-1]
        calls = self.state / "calls"
        self.calls = calls.read_text().splitlines() if calls.exists() else []
        return self

    def called(self, prefix):
        return [call for call in self.calls if call.startswith(prefix)]

    def close(self):
        self.listener.close()


class SteamVisitTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.scenario = Scenario(self.directory.name)

    def tearDown(self):
        self.scenario.close()
        self.directory.cleanup()

    def assertRefused(self, run, reason):
        self.assertEqual(run.said, [f"refused {reason}"])
        self.assertEqual(run.code, 1)
        self.assertFalse(run.called("systemd-run"), "no visit is started")
        self.assertFalse(run.called("loginctl terminate-user"), "nobody is logged out")

    # --- Who may open a visit ---------------------------------------------

    def test_has_the_licence_line(self):
        text = HELPER.read_text()
        self.assertTrue(text.startswith("#!/bin/bash\n# SPDX-License-Identifier: Apache-2.0"))

    def test_only_through_pkexec(self):
        del self.scenario.env["PKEXEC_UID"]
        self.assertRefused(self.scenario.run(), "not-pkexec")

    def test_only_for_a_grown_up(self):
        # polkit refuses first; the helper checks again (ADR-0023).
        for groups in ("guardian cairn-l3", "guardian cairn-l1", "guardian"):
            with self.subTest(groups=groups):
                self.scenario.set("guardian-groups", groups)
                self.assertRefused(self.scenario.run(), "not-a-grown-up")

    def test_only_from_an_active_session(self):
        for state in ("online", "lingering", ""):
            with self.subTest(state=state):
                self.scenario.set("guardian-state", state)
                self.assertRefused(self.scenario.run(), "no-desktop")

    def test_only_on_the_guardians_own_socket(self):
        self.scenario.wayland.unlink()
        self.assertRefused(self.scenario.run(), "no-desktop")

    # --- Which child ---------------------------------------------------------

    def test_never_an_administrator_even_from_a_level_group(self):
        for groups in ("ada cairn-l1 wheel", "ada cairn-l2 cairn-guardian"):
            with self.subTest(groups=groups):
                self.scenario.set("child-groups", groups)
                self.assertRefused(self.scenario.run(), "not-a-child")

    def test_never_an_account_cairn_did_not_make(self):
        self.scenario.set("child-groups", "ada users")
        self.assertRefused(self.scenario.run(), "not-a-child")

    def test_every_level_is_a_child(self):
        for level in ("cairn-l1", "cairn-l2", "cairn-l3", "cairn-l4"):
            with self.subTest(level=level):
                self.scenario.set("child-groups", f"ada {level}")
                self.assertEqual(self.scenario.run().said, ["open", "closing", "closed"])

    def test_only_a_plain_existing_login_name(self):
        for name in ("../ada", "ada b", "-ada", "Ada", "nobody-here"):
            with self.subTest(name=name):
                self.assertRefused(self.scenario.run(child=name), "not-a-child")

    def test_exactly_one_child(self):
        run = self.scenario.run(child="")
        self.assertEqual(run.code, 1)
        self.assertFalse(run.called("systemd-run"))

    # --- When ---------------------------------------------------------------

    def test_one_visit_at_a_time(self):
        self.scenario.visits.write_text("ben\n")
        run = self.scenario.run()
        self.assertRefused(run, "busy")
        self.assertEqual(self.scenario.visits.read_text(), "ben\n", "the other visit's list stays")

    def test_never_while_the_child_is_logged_in(self):
        self.scenario.set("child-sessions", "c3")
        self.scenario.set("session-class", "user")
        self.scenario.set("session-service", "sddm")
        self.assertRefused(self.scenario.run(), "logged-in")
        self.assertFalse(self.scenario.visits.exists(), "the child is not left refused")

    def test_waits_for_the_last_login_to_finish_ending(self):
        self.scenario.set("child-sessions", "c3")
        self.scenario.set("leftover", "2")
        self.assertEqual(self.scenario.run().said, ["waiting", "open", "closing", "closed"])

    def test_refuses_when_the_last_login_never_ends(self):
        self.scenario.set("leftover", "99")
        run = self.scenario.run()
        self.assertEqual(run.said, ["waiting", "refused still-running"])
        self.assertFalse(run.called("systemd-run"))
        self.assertFalse(run.called("pkill"))
        self.assertFalse(self.scenario.visits.exists())

    def test_never_while_the_login_screen_cannot_read_the_list(self):
        # PAM lets the child in when SELinux keeps it from reading the list.
        self.scenario.flag("selinux")
        self.assertRefused(self.scenario.run(), "not-set-up")
        self.assertFalse(self.scenario.visits.exists())

    def test_opens_once_the_list_is_labelled(self):
        self.scenario.flag("selinux")
        self.scenario.flag("labelled")
        self.assertEqual(self.scenario.run().said, ["open", "closing", "closed"])

    # --- The visit ------------------------------------------------------------

    def test_the_child_is_listed_for_the_whole_visit(self):
        run = self.scenario.run()
        self.assertEqual((self.scenario.state / "listed-during-visit").read_text(), "ada\n")
        self.assertFalse(self.scenario.visits.exists(), "and no longer once it closed")
        self.assertIn(f"restorecon {self.scenario.visits}", run.calls)

    def test_cage_runs_as_the_child_on_the_guardians_socket(self):
        run = self.scenario.run()
        [call] = run.called("systemd-run")
        words = call.split()
        self.assertIn(f"--uid=ada", words)
        self.assertIn("--property=PAMName=cairn-steam-visit", words)
        self.assertIn(f"--property=OpenFile={self.scenario.wayland}:wayland", words)
        self.assertIn("--setenv=WAYLAND_SOCKET=3", words)
        self.assertIn("--unit=cairn-steam-visit-%d" % ME, words)
        tail = call.split(" -- ", 1)[1]
        self.assertEqual(tail, "/usr/local/libexec/cairn/cage-visit -- /bin/bash -c exec /usr/bin/steam")

    def test_never_opens_a_socket_from_the_command_line(self):
        run = self.scenario.run(child="/tmp/wayland-0")
        self.assertRefused(run, "not-a-child")

    def test_says_when_cage_never_started(self):
        self.scenario.unflag("cage-starts")
        run = self.scenario.run()
        self.assertEqual(run.said, ["closing", "closed"])
        self.assertEqual(run.code, 1)

    # --- The close ------------------------------------------------------------

    def test_steam_shuts_down_before_the_child_is_ended(self):
        self.scenario.flag("steam-running")
        run = self.scenario.run()
        shutdown = [i for i, c in enumerate(run.calls) if c.startswith("timeout") and "steam -shutdown" in c]
        ended = run.calls.index("loginctl terminate-user ada")
        self.assertEqual(len(shutdown), 1)
        self.assertLess(shutdown[0], ended)
        self.assertIn("--reuid=%d" % ME, run.calls[shutdown[0]])

    def test_a_steam_that_is_not_running_is_never_started(self):
        # `steam -shutdown` starts a client that is not running.
        run = self.scenario.run()
        self.assertFalse([c for c in run.calls if "steam -shutdown" in c])

    def test_the_end_of_input_closes_like_done(self):
        self.assertEqual(self.scenario.run(stdin="").said, ["open", "closing", "closed"])

    def test_other_lines_do_not_close(self):
        # Only "done" or the end of input; the rest is ignored.
        self.assertEqual(self.scenario.run(stdin="hello\ndone\n").said, ["open", "closing", "closed"])

    def test_a_login_that_got_past_pam_ends_the_visit(self):
        self.scenario.flag("login-during-visit")
        run = self.scenario.run(keep_stdin_open=True)
        self.assertEqual(run.said, ["open", "closing", "closed"])
        self.assertIn("loginctl terminate-user ada", run.calls)

    def test_a_signal_closes_the_visit_like_done(self):
        # The window's session ending sends SIGTERM or SIGHUP.
        for signal_name in ("SIGTERM", "SIGHUP", "SIGINT"):
            with self.subTest(signal=signal_name):
                run = self.scenario.run(signal_after_open=getattr(signal, signal_name))
                self.assertEqual(run.said, ["open", "closing", "closed"])
                self.assertIn("loginctl terminate-user ada", run.calls)
                self.assertFalse(self.scenario.visits.exists())

    def test_a_signal_before_the_visit_opens_leaves_nothing_behind(self):
        self.scenario.flag("stop-early")
        self.assertRefused(self.scenario.run(), "stopped")
        self.assertFalse(self.scenario.visits.exists(), "the child is not left refused")

    def test_leftover_x11_files_of_the_child_are_removed(self):
        tmp = self.scenario.state / "tmp"
        (tmp / ".X1-lock").write_text("")
        x_socket = socket.socket(socket.AF_UNIX)
        x_socket.bind(str(tmp / ".X11-unix" / "X1"))
        x_socket.close()
        self.scenario.run()
        self.assertFalse((tmp / ".X1-lock").exists())
        self.assertFalse((tmp / ".X11-unix" / "X1").exists())

    def test_ending_the_child_is_usually_enough(self):
        run = self.scenario.run()
        self.assertEqual(run.said, ["open", "closing", "closed"])
        self.assertFalse(run.called("pkill"))

    def test_what_will_not_end_is_killed(self):
        self.scenario.flag("needs-kill")
        run = self.scenario.run()
        self.assertEqual(run.said, ["open", "closing", "closed"])
        self.assertIn("pkill --signal KILL --uid %d" % ME, run.calls)
        self.assertFalse(self.scenario.visits.exists())

    def test_stuck_keeps_the_child_refused(self):
        self.scenario.flag("visit-survives")
        run = self.scenario.run()
        self.assertEqual(run.said, ["open", "closing", "stuck"])
        self.assertEqual(run.code, 1)
        self.assertIn("pkill --signal KILL --uid %d" % ME, run.calls)
        self.assertEqual(self.scenario.visits.read_text(), "ada\n", "PAM still refuses the child")

    def test_the_journal_says_who_opened_and_closed_it(self):
        run = self.scenario.run()
        notes = [c.split(" -- ", 1)[1] for c in run.called("logger --tag cairn-steam-visit")]
        self.assertEqual(notes, ["guardian opened ada's Steam visit", "guardian closed ada's Steam visit"])

    def test_a_failure_after_listing_takes_the_child_off_the_list(self):
        # chmod fails here; nothing else may be left behind.
        (self.scenario.state / "bin" / "chmod").write_text("#!/bin/bash\nexit 1\n")
        (self.scenario.state / "bin" / "chmod").chmod(0o755)
        run = self.scenario.run()
        self.assertEqual(run.code, 1)
        self.assertFalse(run.called("systemd-run"))
        self.assertFalse(self.scenario.visits.exists(), "the child is not left refused")

    # --- Provisioning's copy of the games (ADR-0033) ---------------------------

    def copy(self, **arguments):
        """Provisioning's copy, of every game unless `appids` names some."""
        self.scenario.env.pop("PKEXEC_UID", None)
        arguments.setdefault("appids", ("--all",))
        return self.scenario.run(copy=True, **arguments)

    def assertCopyRefused(self, run, reason):
        self.assertEqual(run.said, [f"refused {reason}"])
        self.assertEqual(run.code, 1)
        self.assertFalse(run.called("setpriv"), "nothing is copied")
        self.assertFalse(self.scenario.marker.exists(), "the marker goes")

    def test_a_copy_is_never_started_through_pkexec(self):
        # The visit's polkit action must not become a way to start copies.
        run = self.scenario.run(copy=True, appids=("--all",))
        self.assertCopyRefused(run, "not-for-pkexec")
        self.assertFalse(self.scenario.visits.exists())

    def test_a_copy_holds_the_list_and_the_marker_while_it_runs(self):
        run = self.copy()
        self.assertEqual(run.said, ["finished"])
        self.assertEqual(run.code, 0)
        self.assertEqual((self.scenario.state / "listed-during-copy").read_text(), "ada\n")
        self.assertEqual((self.scenario.state / "marked-during-copy").read_text(), "ada\n")
        self.assertIn(f"restorecon {self.scenario.visits}", run.calls)
        self.assertFalse(self.scenario.visits.exists(), "the child can log in again")
        self.assertFalse(self.scenario.marker.exists())

    def test_a_copy_that_fails_still_lets_the_child_in(self):
        self.scenario.set("copy-status", "1")
        run = self.copy()
        self.assertEqual(run.said, ["failed"])
        self.assertEqual(run.code, 1)
        self.assertFalse(self.scenario.visits.exists())
        self.assertFalse(self.scenario.marker.exists())

    def test_the_copy_runs_as_the_child_with_nothing_of_roots(self):
        run = self.copy()
        [call] = run.called("setpriv")
        self.assertEqual(
            call,
            f"setpriv --reuid={ME} --regid={ME} --init-groups env -i HOME={self.scenario.state}/home/ada"
            " USER=ada LOGNAME=ada PATH=/usr/bin:/bin /usr/bin/python3 -I /usr/local/libexec/cairn/cairn-copy-games",
        )
        self.assertFalse(run.called("systemd-run"), "no visit")
        self.assertFalse(run.called("loginctl terminate-user"), "nobody is logged out")

    def test_a_copy_of_a_childs_list_passes_the_appids_on(self):
        run = self.copy(appids=("620", "294690"))
        self.assertEqual(run.said, ["finished"])
        [call] = run.called("setpriv")
        self.assertTrue(
            call.endswith(" /usr/bin/python3 -I /usr/local/libexec/cairn/cairn-copy-games --only 620 294690"),
            call)
        self.assertFalse(self.scenario.visits.exists())
        self.assertFalse(self.scenario.marker.exists())

    def test_all_copies_every_game(self):
        [call] = self.copy(appids=("--all",)).called("setpriv")
        self.assertTrue(call.endswith("/usr/local/libexec/cairn/cairn-copy-games"), call)

    def test_an_empty_list_copies_nothing(self):
        # A parent can save a list with no games; that must never become
        # every game.
        run = self.copy(appids=())
        self.assertEqual(run.said, ["finished"])
        self.assertEqual(run.code, 0)
        self.assertFalse(run.called("setpriv"), "nothing is copied")
        self.assertFalse(self.scenario.visits.exists(), "the child was never listed")
        self.assertFalse(self.scenario.marker.exists())

    def test_all_is_never_mixed_with_a_list(self):
        for appids in (("--all", "620"), ("620", "--all"), ("--all", "--all")):
            with self.subTest(appids=appids):
                self.assertCopyRefused(self.copy(appids=appids), "appid")
                self.assertFalse(self.scenario.visits.exists())

    def test_an_appid_that_is_not_a_plain_number_copies_nothing(self):
        for bad in ("10a", "0620", "-5", "", "620/..", "--only", "12345678901", "620 621"):
            with self.subTest(appid=bad):
                run = self.copy(appids=("620", bad))
                self.assertCopyRefused(run, "appid")
                self.assertFalse(self.scenario.visits.exists(), "the child was never listed")

    def test_a_list_is_never_copied_through_pkexec(self):
        run = self.scenario.run(copy=True, appids=("620",))
        self.assertCopyRefused(run, "not-for-pkexec")
        self.assertFalse(self.scenario.visits.exists())

    def test_a_list_is_refused_while_the_child_is_logged_in(self):
        self.scenario.set("child-sessions", "c2")
        self.scenario.set("session-class", "user")
        self.scenario.set("session-service", "sddm")
        self.assertCopyRefused(self.copy(appids=("620",)), "logged-in")
        self.assertFalse(self.scenario.visits.exists())

    def test_a_list_is_refused_during_a_visit(self):
        self.scenario.visits.write_text("ben\n")
        self.assertCopyRefused(self.copy(appids=("620",)), "busy")

    def test_a_copy_needs_a_child(self):
        self.assertCopyRefused(self.copy(child=None, appids=()), "usage")

    def test_a_copy_refuses_an_empty_name(self):
        self.assertCopyRefused(self.copy(child=""), "not-a-child")

    def test_only_the_copy_programs_own_lines_are_passed_on(self):
        self.scenario.set("copy-says", "\n".join([
            "copied 294690",
            "have 12",
            "skipped 7 beta-branch",
            "no-reflink",
            "/home/ada/.local/share/Steam",
            "copied 294690 and more",
            "copied ../12",
            "skipped 7 Beta Branch",
            "\x1b]0;title\x07",
            "refused busy",
            "finished",
        ]))
        run = self.copy()
        self.assertEqual(run.said, ["copied 294690", "have 12", "skipped 7 beta-branch", "no-reflink", "finished"])
        trace = (self.scenario.state / "trace").read_text()
        self.assertNotIn("/home/ada/.local", trace, "the trace never shows a line before it is checked")
        self.assertNotIn("a word on the error output", trace)

    def notes(self, run):
        return [c.split(" -- ", 1)[1] for c in run.called("logger --tag cairn-steam-visit")]

    def test_a_copy_keeps_a_record_in_the_journal(self):
        # Provisioning prints only sentences of its own, so the journal is
        # where a grown-up finds what happened (docs/TRY.md).
        self.scenario.set("copy-says", "\n".join([
            "copied 294690", "have 12", "skipped 7 copy-failed", "unreadable-config", "/home/ada/x",
        ]))
        run = self.copy()
        self.assertEqual(run.said, ["copied 294690", "have 12", "skipped 7 copy-failed", "unreadable-config",
                                    "finished"])
        self.assertEqual(self.notes(run), [
            "copying ada's games",
            "copying ada's games: copied 294690",
            "copying ada's games: skipped 7 copy-failed",
            "copying ada's games: unreadable-config",
            "finished copying ada's games",
        ])

    def test_a_failed_copy_says_how_it_ended_in_the_journal(self):
        self.scenario.set("copy-status", "3")
        run = self.copy()
        self.assertEqual(run.said, ["failed"])
        self.assertEqual(self.notes(run), [
            "copying ada's games",
            "copying ada's games failed: the copy program ended with status 3",
        ])

    def test_a_refused_copy_says_why_in_the_journal(self):
        self.scenario.set("child-sessions", "c3")
        self.scenario.set("session-class", "user")
        self.scenario.set("session-service", "sddm")
        self.assertEqual(self.notes(self.copy()), ["no games were copied for ada: refused logged-in"])
        # A name that is not a child's is never written down.
        self.assertEqual(self.notes(self.copy(child="../ada")), ["no games were copied for a child: refused not-a-child"])

    def test_a_refused_visit_writes_nothing_to_the_journal(self):
        self.scenario.visits.write_text("ben\n")
        run = self.scenario.run()
        self.assertRefused(run, "busy")
        self.assertEqual(self.notes(run), [])

    def test_no_copy_while_a_visit_is_open(self):
        self.scenario.visits.write_text("ben\n")
        self.assertCopyRefused(self.copy(), "busy")
        self.assertEqual(self.scenario.visits.read_text(), "ben\n", "the visit's list stays")

    def test_one_copy_at_a_time(self):
        self.scenario.marker.write_text("ben\n")
        run = self.copy()
        self.assertEqual(run.said, ["refused busy"])
        self.assertEqual(self.scenario.marker.read_text(), "ben\n", "the other copy's marker stays")
        self.assertFalse(self.scenario.visits.exists())

    def test_no_copy_while_the_child_is_logged_in(self):
        self.scenario.set("child-sessions", "c3")
        self.scenario.set("session-class", "user")
        self.scenario.set("session-service", "sddm")
        self.assertCopyRefused(self.copy(), "logged-in")
        self.assertFalse(self.scenario.visits.exists(), "the child is not left refused")

    def test_a_copy_waits_for_the_last_login_to_finish_ending(self):
        self.scenario.set("leftover", "2")
        self.assertEqual(self.copy().said, ["waiting", "finished"])

    def test_a_copy_refuses_when_the_last_login_never_ends(self):
        self.scenario.set("leftover", "99")
        run = self.copy()
        self.assertEqual(run.said, ["waiting", "refused still-running"])
        self.assertFalse(run.called("setpriv"))
        self.assertFalse(run.called("pkill"), "a copy never kills anything")
        self.assertFalse(self.scenario.visits.exists())
        self.assertFalse(self.scenario.marker.exists())

    def test_a_copy_needs_the_label_a_visit_needs(self):
        self.scenario.flag("selinux")
        self.assertCopyRefused(self.copy(), "not-set-up")
        self.assertFalse(self.scenario.visits.exists())

    def test_a_copy_is_only_for_a_child(self):
        self.scenario.set("child-groups", "ada cairn-l1 wheel")
        self.assertCopyRefused(self.copy(), "not-a-child")
        self.assertCopyRefused(self.copy(child="../ada"), "not-a-child")

    def test_a_signal_before_the_copy_leaves_nothing_behind(self):
        self.scenario.flag("stop-early")
        self.assertCopyRefused(self.copy(), "stopped")
        self.assertFalse(self.scenario.visits.exists())

    def test_a_visit_during_a_copy_says_games_are_being_copied(self):
        self.scenario.visits.write_text("ben\n")
        self.scenario.marker.write_text("ben\n")
        run = self.scenario.run()
        self.assertRefused(run, "copying")
        self.assertEqual(self.scenario.visits.read_text(), "ben\n", "the copy's list stays")
        self.assertTrue(self.scenario.marker.exists(), "and so does its marker")

    def test_a_marker_without_a_list_does_not_stop_a_visit(self):
        # Left by a copy that was killed outright; /run empties at a restart.
        self.scenario.marker.write_text("ben\n")
        self.assertEqual(self.scenario.run().said, ["open", "closing", "closed"])

    # --- Where things are -----------------------------------------------------

    def test_provisioning_installs_what_the_helper_runs(self):
        text = PROVISION.read_text()
        for path in ('"$PREFIX/libexec/cairn/cage-visit"', '"$PREFIX/libexec/cairn/cairn-steam-visit"'):
            self.assertTrue(path in text, f"provisioning installs {path}")
        helper = HELPER.read_text()
        self.assertIn("CAGE=/usr/local/libexec/cairn/cage-visit\n", helper)

    def test_provisioning_installs_the_copy_program_the_helper_runs(self):
        self.assertIn('"$PREFIX/libexec/cairn/cairn-copy-games"', PROVISION.read_text())
        self.assertIn("COPY_GAMES=/usr/local/libexec/cairn/cairn-copy-games\n", HELPER.read_text())


if __name__ == "__main__":
    unittest.main()
