#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Tests for the shared boot and provisioning faillock repair (#301)."""

import importlib.machinery
import importlib.util
import os
import pathlib
import pwd
import stat
import subprocess
import tempfile
import types
import unittest
from unittest import mock

HERE = pathlib.Path(__file__).resolve().parents[2]
HELPER = HERE / "system_files/usr/libexec/cairn/cairn-reconcile-faillock"


def load_helper():
    loader = importlib.machinery.SourceFileLoader("cairn_reconcile_faillock", str(HELPER))
    spec = importlib.util.spec_from_loader(loader.name, loader)
    if spec is None:
        raise RuntimeError(f"could not load {HELPER}")
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


class FaillockReconcileTest(unittest.TestCase):
    def setUp(self):
        self.module = load_helper()
        self.directory = tempfile.TemporaryDirectory()
        self.folder = pathlib.Path(self.directory.name)
        self.guardian = self.folder / "guardian"
        self.guardian.write_bytes(b"count")
        self.owner = os.stat(self.guardian).st_uid
        self.account = types.SimpleNamespace(pw_uid=self.owner + 1000)
        self.sddm = types.SimpleNamespace(pw_uid=self.owner + 2000)

    def tearDown(self):
        self.directory.cleanup()

    def account_lookup(self, name):
        if name == "guardian":
            return self.account
        if name == "sddm":
            return self.sddm
        raise KeyError(name)

    @staticmethod
    def completed(stdout=""):
        return subprocess.CompletedProcess([], 0, stdout=stdout)

    def run_reconcile(self, acl="user::rw-\nuser:child:rw-\ngroup::rw-\nmask::rw-\nother::---\n"):
        calls = []

        def run(command, **kwargs):
            calls.append((command, kwargs))
            if command[0] == "getfacl":
                return self.completed(acl)
            return self.completed()

        with (
            mock.patch.object(self.module.pwd, "getpwnam", side_effect=self.account_lookup),
            mock.patch.object(self.module.os, "fchown") as fchown,
            mock.patch.object(self.module.os, "fchmod") as fchmod,
            mock.patch.object(self.module.subprocess, "run", side_effect=run),
        ):
            result = self.module.reconcile(self.folder)
        return result, calls, fchown, fchmod

    def test_wrong_owner_and_child_write_acl_are_replaced(self):
        result, calls, fchown, fchmod = self.run_reconcile()

        self.assertEqual(result, [])
        fchown.assert_called_once_with(mock.ANY, self.account.pw_uid, 0)
        fchmod.assert_called_once_with(mock.ANY, 0o660)
        set_acl = [command for command, _ in calls if command[0] == "setfacl"]
        self.assertEqual(len(set_acl), 1)
        self.assertIn(
            "--set=u::rw-,u:%d:r--,g::rw-,m::rw-,o::---" % self.sddm.pw_uid,
            set_acl[0],
        )
        self.assertNotIn("child", " ".join(set_acl[0]))

    def test_exact_policy_is_idempotent(self):
        os.chmod(self.guardian, 0o660)
        self.account.pw_uid = self.owner
        acl = (
            "user::rw-\n"
            f"user:{self.sddm.pw_uid}:r--\n"
            "group::rw-\nmask::rw-\nother::---\n"
        )
        real = os.stat(self.guardian)
        exact = types.SimpleNamespace(
            st_uid=self.owner,
            st_gid=0,
            st_mode=stat.S_IFREG | 0o660,
            st_dev=real.st_dev,
            st_ino=real.st_ino,
        )
        with mock.patch.object(self.module.os, "fstat", return_value=exact):
            result, calls, fchown, fchmod = self.run_reconcile(acl)

        self.assertEqual(result, [])
        fchown.assert_not_called()
        fchmod.assert_not_called()
        self.assertFalse([command for command, _ in calls if command[0] == "setfacl"])

    def test_symlink_is_refused_without_touching_its_target(self):
        self.guardian.unlink()
        target = self.folder / "target"
        target.write_text("safe")
        self.guardian.symlink_to(target)

        with mock.patch.object(self.module.pwd, "getpwnam", side_effect=self.account_lookup):
            errors = self.module.reconcile(self.folder)

        self.assertEqual(target.read_text(), "safe")
        self.assertTrue(any("guardian" in error and "symlink" in error for error in errors), errors)

    def test_non_regular_file_is_refused(self):
        self.guardian.unlink()
        self.guardian.mkdir()

        with mock.patch.object(self.module.pwd, "getpwnam", side_effect=self.account_lookup):
            errors = self.module.reconcile(self.folder)

        self.assertTrue(any("guardian" in error and "regular" in error for error in errors), errors)

    def test_missing_account_is_refused(self):
        def lookup(name):
            if name == "sddm":
                return self.sddm
            raise KeyError(name)

        with mock.patch.object(self.module.pwd, "getpwnam", side_effect=lookup):
            errors = self.module.reconcile(self.folder)

        self.assertTrue(any("guardian" in error and "account" in error for error in errors), errors)

    def test_path_replacement_is_refused_before_mutation(self):
        actual_stat = self.module.os.stat
        calls = 0

        def replaced(path, **kwargs):
            nonlocal calls
            value = actual_stat(path, **kwargs)
            calls += 1
            if calls == 1:
                return types.SimpleNamespace(st_dev=value.st_dev, st_ino=value.st_ino + 1)
            return value

        with (
            mock.patch.object(self.module.pwd, "getpwnam", side_effect=self.account_lookup),
            mock.patch.object(self.module.os, "stat", side_effect=replaced),
            mock.patch.object(self.module.os, "fchown") as fchown,
        ):
            errors = self.module.reconcile(self.folder)

        self.assertTrue(any("guardian" in error and "replaced" in error for error in errors), errors)
        fchown.assert_not_called()

    def test_acl_failure_stops_with_an_error(self):
        def run(command, **kwargs):
            if command[0] == "getfacl":
                return self.completed("user::rw-\ngroup::rw-\nother::---\n")
            raise subprocess.CalledProcessError(1, command)

        with (
            mock.patch.object(self.module.pwd, "getpwnam", side_effect=self.account_lookup),
            mock.patch.object(self.module.os, "fchown"),
            mock.patch.object(self.module.os, "fchmod"),
            mock.patch.object(self.module.subprocess, "run", side_effect=run),
        ):
            errors = self.module.reconcile(self.folder)

        self.assertTrue(any("guardian" in error and "setfacl" in error for error in errors), errors)


if __name__ == "__main__":
    unittest.main()
