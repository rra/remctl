# test_remctl.py -- Test suite for remctl Python bindings
#
# Written by Russ Allbery <eagle@eyrie.org>
# Copyright 2019 Russ Allbery <eagle@eyrie.org>
# Copyright 2008, 2011-2012, 2014
#     The Board of Trustees of the Leland Stanford Junior University
#
# SPDX-License-Identifier: MIT

import errno
import os
import re
import signal
import time
import unittest
from typing import Any, Callable

import remctl


def needs_kerberos(func):
    # type: (Callable[..., Any]) -> Callable[..., Any]
    """unittest test method decorator to skip tests requiring Kerberos

    Used to annotate test methods that require a Kerberos configuration.
    Ignores failures in the annotated test method.
    """

    def wrapper(*args, **kw):
        # type: (*Any, **Any) -> Any
        if not os.path.isfile("tests/config/principal"):
            return True
        return func(*args, **kw)

    wrapper.__name__ = func.__name__
    wrapper.__doc__ = func.__doc__
    return wrapper


class TestRemctl(unittest.TestCase):
    def get_principal(self):
        # type: () -> str
        file = open("tests/config/principal", "r")
        principal = file.read().rstrip()
        file.close()
        return principal

    @needs_kerberos
    def start_remctld(self):
        # type: () -> None
        try:
            os.mkdir("tests/tmp")
            os.remove("tests/tmp/pid")
        except OSError as e:
            if e.errno != errno.ENOENT and e.errno != errno.EEXIST:
                raise
        principal = self.get_principal()
        child = os.fork()
        if child == 0:
            output = open("tests/tmp/output", "w")
            os.dup2(output.fileno(), 1)
            os.dup2(output.fileno(), 2)
            remctld = os.environ.get("REMCTLD", "remctld")
            os.execl(
                remctld,
                "remctld",
                "-m",
                "-p",
                "14373",
                "-s",
                principal,
                "-f",
                "tests/data/remctl.conf",
                "-P",
                "tests/tmp/pid",
                "-d",
                "-S",
                "-F",
                "-k",
                "tests/config/keytab",
            )
        if not os.path.isfile("tests/tmp/pid"):
            time.sleep(1)

    def stop_remctld(self):
        # type: () -> None
        try:
            file = open("tests/tmp/pid", "r")
            pid = file.read().rstrip()
            file.close()
            os.kill(int(pid), signal.SIGTERM)
            child, status = os.waitpid(int(pid), 0)
            os.remove("tests/tmp/pid")
        except (IOError, OSError) as e:
            if e.errno != errno.ENOENT:
                raise

    @needs_kerberos
    def run_kinit(self):
        # type: () -> bool
        os.environ["KRB5CCNAME"] = "tests/tmp/krb5cc_test"
        self.principal = self.get_principal()
        commands = (
            "kinit --no-afslog -k -t tests/config/keytab " + self.principal,
            "kinit -k -t tests/config/keytab " + self.principal,
            "kinit -t tests/config/keytab " + self.principal,
            "kinit -k -K tests/config/keytab " + self.principal,
        )
        for command in commands:
            if os.system(command + " >/dev/null 2>&1 </dev/null") == 0:
                return True
        if not os.path.isfile("tests/tmp/pid"):
            time.sleep(1)
        self.stop_remctld()
        return False

    def setUp(self):
        # type: () -> None
        self.start_remctld()
        assert self.run_kinit()

    @needs_kerberos
    def tearDown(self):
        # type: () -> None
        self.stop_remctld()
        os.remove("tests/tmp/output")
        try:
            os.remove("tests/tmp/krb5cc_test")
            os.rmdir("tests/tmp")
        except OSError as e:
            if e.errno != errno.ENOENT:
                raise


class TestRemctlSimple(TestRemctl):
    @needs_kerberos
    def test_simple_success(self):
        # type: () -> None
        command = ("test", "test")
        result = remctl.remctl("localhost", 14373, self.principal, command)
        self.assertEqual(result.stdout, b"hello world\n")
        self.assertEqual(result.stderr, None)
        self.assertEqual(result.status, 0)

    @needs_kerberos
    def test_simple_status(self):
        # type: () -> None
        command = ["test", "status", "2"]
        result = remctl.remctl(
            host="localhost",
            command=command,
            port="14373",
            principal=self.principal,
        )
        self.assertEqual(result.stdout, None)
        self.assertEqual(result.stderr, None)
        self.assertEqual(result.status, 2)

    @needs_kerberos
    def test_simple_failure(self):
        # type: () -> None
        command = ("test", "bad-command")
        try:
            remctl.remctl("localhost", 14373, self.principal, command)
        except remctl.RemctlProtocolError as error:
            self.assertEqual(str(error), str(b"Unknown command"))

    @needs_kerberos
    def test_simple_errors(self):
        # type: () -> None
        try:
            remctl.remctl("localhost")
        except ValueError as error:
            self.assertEqual(str(error), "command must not be empty")
        try:
            remctl.remctl(host="localhost", command="foo")
        except TypeError as error:
            self.assertEqual(
                str(error), "command must be a sequence or iterator"
            )
        try:
            remctl.remctl("localhost", "foo", self.principal, [])
        except TypeError as error:
            self.assertEqual(str(error), "port must be a number: 'foo'")
        try:
            remctl.remctl("localhost", -1, self.principal, [])
        except ValueError as error:
            self.assertEqual(str(error), "invalid port number: -1")
        try:
            remctl.remctl("localhost", 14373, self.principal, [])
        except ValueError as error:
            self.assertEqual(str(error), "command must not be empty")
        try:
            remctl.remctl("localhost", 14373, self.principal, "test")
        except TypeError as error:
            self.assertEqual(
                str(error), "command must be a sequence or iterator"
            )


class TestRemctlFull(TestRemctl):
    @needs_kerberos
    def test_full_success(self):
        # type: () -> None
        r = remctl.Remctl()
        r.open("localhost", 14373, self.principal)
        r.command(["test", "test"])
        type, data, stream, status, error = r.output()
        self.assertEqual(type, "output")
        self.assertEqual(data, b"hello world\n")
        self.assertEqual(stream, 1)
        type, data, stream, status, error = r.output()
        self.assertEqual(type, "status")
        self.assertEqual(status, 0)
        type, data, stream, status, error = r.output()
        self.assertEqual(type, "done")
        r.noop()
        r.close()

    @needs_kerberos
    def test_full_failure(self):
        # type: () -> None
        r = remctl.Remctl("localhost", 14373, self.principal)
        r.command(["test", "bad-command"])
        type, data, stream, status, error = r.output()
        self.assertEqual(type, "error")
        self.assertEqual(data, b"Unknown command")
        self.assertEqual(error, 5)

    @needs_kerberos
    def test_ccache(self):
        # type: () -> None
        r = remctl.Remctl()
        os.environ["KRB5CCNAME"] = "nonexistent-file"
        try:
            r.open("localhost", 14373, self.principal)
            self.fail("open without ticket cache succeeded")
        except remctl.RemctlError:
            pass
        okay = False
        try:
            r.set_ccache("tests/tmp/krb5cc_test")
            okay = True
        except remctl.RemctlError:
            pass
        if okay:
            r.open("localhost", 14373, self.principal)
        r.close()

    @needs_kerberos
    def test_source_ip(self):
        # type: () -> None
        r = remctl.Remctl()
        r.set_source_ip("127.0.0.1")
        r.open("127.0.0.1", 14373, self.principal)
        r.set_source_ip("::1")
        pattern = "(cannot connect to|unknown host) .*"
        try:
            r.open("127.0.0.1", 14373, self.principal)
        except remctl.RemctlError as error:
            self.assertTrue(re.compile(pattern).match(str(error)))

    @needs_kerberos
    def test_timeout(self):
        # type: () -> None
        r = remctl.Remctl()
        r.set_timeout(1)
        r.open("127.0.0.1", 14373, self.principal)
        r.command(["test", "sleep"])
        try:
            type, data, stream, status, error = r.output()
            assert "output unexpectedly succeeded"
        except remctl.RemctlError:
            self.assertEqual(r.error(), "error receiving token: timed out")
        r.close()

    @needs_kerberos
    def test_full_errors(self):
        # type: () -> None
        r = remctl.Remctl()
        try:
            r.open("localhost", "foo")
        except TypeError as error:
            self.assertEqual(str(error), "port must be a number: 'foo'")
        try:
            r.open("localhost", -1)
        except ValueError as error:
            self.assertEqual(str(error), "invalid port number: -1")
        pattern = "cannot connect to localhost \\(port 14444\\): .*"
        try:
            r.open("localhost", 14444)
        except remctl.RemctlError as error:
            self.assertTrue(re.compile(pattern).match(str(error)))
        self.assertTrue(re.compile(pattern).match(r.error()))
        try:
            r.command(["test", "test"])
        except remctl.RemctlNotOpenedError as error:
            self.assertEqual(str(error), "no currently open connection")
        r.open("localhost", 14373, self.principal)
        try:
            r.command("test")
        except TypeError as error:
            self.assertEqual(
                str(error), "command must be a sequence or iterator"
            )
        try:
            r.command([])
        except ValueError as error:
            self.assertEqual(str(error), "command must not be empty")
        r.close()
        try:
            r.output()
        except remctl.RemctlNotOpenedError as error:
            self.assertEqual(str(error), "no currently open connection")
        self.assertEqual(r.error(), "no currently open connection")


if __name__ == "__main__":
    unittest.main()
