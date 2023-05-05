# Runs simulations and tests for the system.
import unittest

from toyssd.system import System


class TestSystem(unittest.TestCase):

    def setUp(self):
        self.system = System()

    def test_create_system(self):
        self.assertIsInstance(self.system, System)
        pass

    def test_run_write_read_workload(self):
        """Write some data to storage and read it back."""
        result = self.system.env.process(self.system.run_write_read_workload(2))
        self.system.env.run()
        self.assertEqual(result.writes, 2)
        self.assertEqual(result.reads, 2)
        self.assertEqual(result.elapsed_time,
                         (result.writes * 2) + (result.reads * 2))

        pass
