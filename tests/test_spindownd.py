"""Unit tests for spindownd, with a fake clock and fake disks.

Run with: python3 -m unittest discover -s tests -v
"""

import importlib.machinery
import importlib.util
import json
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
SPINDOWND = os.path.join(HERE, os.pardir, "usr", "sbin", "spindownd")

# Do not drop a __pycache__ into usr/sbin (it would end up in the .deb).
sys.dont_write_bytecode = True
_loader = importlib.machinery.SourceFileLoader("spindownd", SPINDOWND)
_spec = importlib.util.spec_from_loader("spindownd", _loader)
spindownd = importlib.util.module_from_spec(_spec)
_loader.exec_module(spindownd)
spindownd.log = lambda message: None


class FakeDisks:
    """Disks whose counters, presence and sleep state the test controls."""

    def __init__(self, **names):
        # devicefile -> kernel name; kernel name -> counter
        self.names = {"/dev/disk/by-id/" + k: v for k, v in names.items()}
        self.io = {v: 100 for v in names.values()}
        self.asleep = set()
        self.sent = []
        self.result = None

    def kernel_name(self, devicefile):
        return self.names.get(devicefile)

    def io_count(self, name):
        return self.io.get(name)

    def disk_model(self, name):
        return "MODEL " + name

    def send_standby(self, devicefile):
        self.sent.append(devicefile)
        if self.result:
            return self.result
        name = self.names[devicefile]
        if name in self.asleep:
            return "asleep"
        self.asleep.add(name)
        return "spun_down"

    def access(self, name):
        self.io[name] += 1
        self.asleep.discard(name)


class Clock:
    def __init__(self):
        self.t = 1000.0

    def __call__(self):
        return self.t


def make(disks, fake, clock):
    return spindownd.Watcher(
        [{"devicefile": "/dev/disk/by-id/" + d, "seconds": s}
         for d, s in disks],
        monotonic=clock, wall=lambda: 1700000000 + clock.t,
        io_count=fake.io_count, disk_model=fake.disk_model,
        kernel_name=fake.kernel_name, send_standby=fake.send_standby)


def run(watcher, clock, seconds, step=30):
    end = clock.t + seconds
    while clock.t < end:
        clock.t += step
        watcher.poll()


class StandbySeconds(unittest.TestCase):
    def test_encoding(self):
        f = spindownd.standby_seconds
        self.assertEqual(f(60), 300)      # 5 minutes
        self.assertEqual(f(120), 600)     # 10 minutes
        self.assertEqual(f(240), 1200)    # 20 minutes
        self.assertEqual(f(241), 1800)    # 30 minutes
        self.assertEqual(f(244), 7200)    # 120 minutes
        self.assertEqual(f(251), 19800)   # 330 minutes
        self.assertEqual(f(252), 1260)
        self.assertEqual(f(255), 1275)

    def test_off_or_undefined(self):
        f = spindownd.standby_seconds
        for value in (0, 253, 254, 256, -1, None, "x"):
            self.assertIsNone(f(value), value)


class WatchLoop(unittest.TestCase):
    def setUp(self):
        self.fake = FakeDisks(a="sda", b="sdb")
        self.clock = Clock()
        self.w = make([("a", 600), ("b", 1200)], self.fake, self.clock)
        self.w.poll()

    def test_spins_down_after_idle_time(self):
        run(self.w, self.clock, 570)
        self.assertEqual(self.fake.sent, [])
        run(self.w, self.clock, 30)
        self.assertEqual(self.fake.sent, ["/dev/disk/by-id/a"])
        disk = self.w.status()["disks"][0]
        self.assertEqual(disk["state"], "standby")
        self.assertEqual(disk["spindowns"], 1)
        self.assertIsNotNone(disk["last_standby"])

    def test_each_disk_its_own_time(self):
        run(self.w, self.clock, 600)
        self.assertNotIn("/dev/disk/by-id/b", self.fake.sent)
        run(self.w, self.clock, 600)
        self.assertIn("/dev/disk/by-id/b", self.fake.sent)

    def test_io_restarts_the_wait(self):
        run(self.w, self.clock, 300)
        self.fake.access("sda")
        run(self.w, self.clock, 570)
        self.assertEqual(self.fake.sent, [])
        self.assertEqual(self.w.status()["disks"][0]["state"], "active")
        run(self.w, self.clock, 60)
        self.assertEqual(self.fake.sent, ["/dev/disk/by-id/a"])

    def test_read_only_wake_spins_down_again(self):
        # The case Debian's hd-idle 1.21 missed on the reference NAS.
        run(self.w, self.clock, 600)
        self.fake.access("sda")      # a read wakes it
        run(self.w, self.clock, 630)
        self.assertEqual(self.fake.sent.count("/dev/disk/by-id/a"), 2)
        self.assertEqual(self.w.status()["disks"][0]["spindowns"], 2)

    def test_idle_disk_only_asked_once_per_spindown_time(self):
        run(self.w, self.clock, 600)
        run(self.w, self.clock, 570)
        self.assertEqual(self.fake.sent.count("/dev/disk/by-id/a"), 1)
        # Still idle a spindown time later: ask again (a S.M.A.R.T. query
        # may have woken it); already asleep, so nothing changes.
        run(self.w, self.clock, 30)
        self.assertEqual(self.fake.sent.count("/dev/disk/by-id/a"), 2)
        self.assertEqual(self.w.status()["disks"][0]["spindowns"], 1)

    def test_woken_without_io_is_spun_down_again(self):
        run(self.w, self.clock, 600)
        self.fake.asleep.discard("sda")   # woken by a command, no I/O
        run(self.w, self.clock, 600)
        self.assertEqual(self.w.status()["disks"][0]["spindowns"], 2)

    def test_failure_reported(self):
        self.fake.result = "failed"
        run(self.w, self.clock, 600)
        self.assertEqual(self.w.status()["disks"][0]["state"], "failed")

    def test_missing_disk_then_back(self):
        del self.fake.names["/dev/disk/by-id/a"]
        run(self.w, self.clock, 900)
        disk = self.w.status()["disks"][0]
        self.assertEqual(disk["state"], "missing")
        self.assertIsNone(disk["idle"])
        self.assertNotIn("/dev/disk/by-id/a", self.fake.sent)
        self.fake.names["/dev/disk/by-id/a"] = "sdc"
        self.fake.io["sdc"] = 5
        run(self.w, self.clock, 30)
        disk = self.w.status()["disks"][0]
        self.assertEqual((disk["device"], disk["state"]), ("sdc", "unknown"))
        run(self.w, self.clock, 600)
        self.assertIn("/dev/disk/by-id/a", self.fake.sent)

    def test_status_fields(self):
        run(self.w, self.clock, 120)
        disk = self.w.status()["disks"][0]
        self.assertEqual(disk["device"], "sda")
        self.assertEqual(disk["model"], "MODEL sda")
        self.assertEqual(disk["timeout"], 600)
        self.assertEqual(disk["idle"], 120)
        self.assertEqual(disk["state"], "unknown")


class Config(unittest.TestCase):
    def test_load(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "spindownd.json")
            with open(path, "w") as f:
                json.dump({"enable": True, "disks": [
                    {"devicefile": "/dev/disk/by-id/a", "spindowntime": 120},
                    {"devicefile": "/dev/disk/by-id/a", "spindowntime": 240},
                    {"devicefile": "/dev/disk/by-id/b", "spindowntime": 0},
                    {"devicefile": "", "spindowntime": 120},
                    {"devicefile": "/dev/disk/by-id/c", "spindowntime": 241},
                ]}, f)
            config, disks = spindownd.load_config(path)
        self.assertTrue(config["enable"])
        self.assertEqual(disks, [
            {"devicefile": "/dev/disk/by-id/a", "seconds": 600},
            {"devicefile": "/dev/disk/by-id/c", "seconds": 1800}])


class SysfsCounters(unittest.TestCase):
    def test_io_count_sums_requests(self):
        with tempfile.TemporaryDirectory() as tmp:
            os.makedirs(os.path.join(tmp, "sda"))
            fields = ["0"] * 17
            fields[0], fields[4], fields[11], fields[15] = "10", "20", "3", "4"
            fields[2] = "999"   # sectors read: not a request count
            with open(os.path.join(tmp, "sda", "stat"), "w") as f:
                f.write("  ".join(fields) + "\n")
            self.assertEqual(spindownd.io_count("sda", tmp), 37)
            self.assertIsNone(spindownd.io_count("sdz", tmp))

    def test_io_count_old_kernel(self):
        with tempfile.TemporaryDirectory() as tmp:
            os.makedirs(os.path.join(tmp, "sda"))
            with open(os.path.join(tmp, "sda", "stat"), "w") as f:
                f.write("5 0 0 0 7 0 0 0 0 0 0\n")
            self.assertEqual(spindownd.io_count("sda", tmp), 12)


class StatusCommand(unittest.TestCase):
    def test_status_json_without_daemon(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = os.path.join(tmp, "out")
            stdout = sys.stdout
            with open(out, "w") as sys.stdout:
                rc = spindownd.main(["--status-file",
                                     os.path.join(tmp, "none.json"),
                                     "status", "--json"])
            sys.stdout = stdout
            self.assertEqual(rc, 0)
            with open(out) as f:
                self.assertEqual(json.load(f), {})


if __name__ == "__main__":
    unittest.main()
