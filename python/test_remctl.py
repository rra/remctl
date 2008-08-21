# test_remctl.py -- Test suite for remctl Python bindings
#
# Written by Russ Allbery <rra@stanford.edu>
# Copyright 2008 Board of Trustees, Leland Stanford Jr. University
#
# See LICENSE for licensing terms.

import remctl
import errno, os, re, signal, time, unittest

class TestRemctl(unittest.TestCase):
    def change_directory(self):
        if os.path.isfile('tests/data/conf-simple'):
            os.chdir('tests')
        elif os.path.isfile('../tests/data/conf-simple'):
            os.chdir('../tests')
        elif os.path.isfile('../../tests/data/conf-simple'):
            os.chdir('../../tests')
        assert(os.path.isfile('data/conf-simple'))

    def get_principal(self):
        file = open('data/test.principal', 'r')
        principal = file.read().rstrip()
        file.close()
        return principal

    def start_remctld(self):
        try:
            os.remove('data/pid')
        except OSError, (error, strerror):
            if error != errno.ENOENT:
                raise
        principal = self.get_principal()
        child = os.fork()
        if child == 0:
            output = open('data/test.output', 'w')
            os.dup2(output.fileno(), 1)
            os.execl('../server/remctld', 'remctld', '-m', '-p', '14373',
                     '-s', principal, '-P', 'data/pid', '-f',
                     'data/conf-simple', '-d', '-S', '-F', '-k',
                     'data/test.keytab')
        if not os.path.isfile('data/pid'):
            time.sleep(1)

    def stop_remctld(self):
        try:
            file = open('data/pid', 'r')
            pid = file.read().rstrip()
            file.close()
            os.kill(int(pid), signal.SIGTERM)
            child, status = os.waitpid(int(pid), 0)
        except IOError, (error, strerror):
            if error != errno.ENOENT:
                raise

    def run_kinit(self):
        os.environ['KRB5CCNAME'] = 'data/test.cache'
        self.principal = self.get_principal()
        commands = ('kinit -k -t data/test.keytab ' + self.principal,
                    'kinit -t data/test.keytab ' + self.principal,
                    'kinit -k -K data/test.keytab ' + self.principal)
        for command in commands:
            if os.system(command + ' >/dev/null </dev/null') == 0:
                return True
        if not os.path.isfile('data/pid'):
            time.sleep(1)
        stop_remctld()
        return False

    def setUp(self):
        self.change_directory()
        self.start_remctld()
        assert(self.run_kinit())

    def tearDown(self):
        self.stop_remctld()
        os.remove('data/test.output')
        try:
            os.remove('data/test.cache')
        except OSError, (error, strerror):
            if error != errno.ENOENT:
                raise

class TestRemctlSimple(TestRemctl):
    def test_simple_success(self):
        command = ('test', 'test')
        result = remctl.remctl('localhost', 14373, self.principal, command)
        self.assertEqual(result.stdout, "hello world\n")
        self.assertEqual(result.stderr, None)
        self.assertEqual(result.status, 0)

    def test_simple_failure(self):
        command = ('test', 'bad-command')
        try:
            result = remctl.remctl('localhost', 14373, self.principal, command)
        except remctl.RemctlProtocolError, error:
            self.assertEqual(str(error), 'Unknown command')

    def test_simple_errors(self):
        try:
            remctl.remctl()
        except remctl.RemctlArgError, error:
            self.assertEqual(str(error), 'no host specified')
        try:
            remctl.remctl('localhost', "foo")
        except remctl.RemctlArgError, error:
            self.assertEqual(str(error), 'port must be a number')
        try:
            remctl.remctl('localhost', -1)
        except remctl.RemctlArgError, error:
            self.assertEqual(str(error), 'invalid port number')
        try:
            remctl.remctl('localhost', 14373, self.principal, [])
        except remctl.RemctlArgError, error:
            self.assertEqual(str(error),
                             'must have two or more arguments to command')

class TestRemctlFull(TestRemctl):
    def test_full_success(self):
        r = remctl.Remctl()
        r.open('localhost', 14373, self.principal)
        r.command(['test', 'test'])
        type, data, stream, status, error = r.output()
        self.assertEqual(type, remctl.REMCTL_OUT_OUTPUT)
        self.assertEqual(data, "hello world\n")
        self.assertEqual(stream, 1)
        type, data, stream, status, error = r.output()
        self.assertEqual(type, remctl.REMCTL_OUT_STATUS)
        self.assertEqual(status, 0)
        type, data, stream, status, error = r.output()
        self.assertEqual(type, remctl.REMCTL_OUT_DONE)
        r.close()

    def test_full_failure(self):
        r = remctl.Remctl('localhost', 14373, self.principal)
        r.command(['test', 'bad-command'])
        type, data, stream, status, error = r.output()
        self.assertEqual(type, remctl.REMCTL_OUT_ERROR)
        self.assertEqual(data, 'Unknown command')
        self.assertEqual(error, 5)

    def test_full_errors(self):
        r = remctl.Remctl()
        try:
            r.open()
        except remctl.RemctlArgError, error:
            self.assertEqual(str(error), 'no host specified')
        try:
            r.open('localhost', 'foo')
        except remctl.RemctlArgError, error:
            self.assertEqual(str(error), 'port must be a number')
        try:
            r.open('localhost', -1)
        except remctl.RemctlArgError, error:
            self.assertEqual(str(error), 'invalid port number')
        try:
            r.open('localhost', 14444)
        except remctl.RemctlError, error:
            self.assertEqual(str(error), 'error opening connection')
        error = 'cannot connect to localhost \(port 14444\): .*'
        self.assert_(re.compile(error).match(r.error()))
        try:
            r.command(['test', 'test'])
        except remctl.RemctlNotOpened, error:
            self.assertEqual(str(error), 'no currently open connection')
        r.open('localhost', 14373, self.principal)
        try:
            r.command('test')
        except remctl.RemctlArgError, error:
            self.assertEqual(str(error), 'you must supply a list of commands')
        try:
            r.command([])
        except remctl.RemctlArgError, error:
            self.assertEqual(str(error),
                             'must have two or more arguments to command')
        r.close()
        try:
            r.output()
        except remctl.RemctlNotOpened, error:
            self.assertEqual(str(error), 'no currently open connection')
        self.assertEqual(r.error(), 'no currently open connection')

if __name__ == '__main__':
    unittest.main()
