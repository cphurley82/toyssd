# Runs simulations and tests for the system.
import unittest
import simpy

from toyssd.system import System


class TestSystem(unittest.TestCase):

    def setUp(self):
        self.env = simpy.Environment()

    def test_create_system(self):
        self.assertIsInstance(self.system, System)
        pass

    def test_run_write_read_workload(self):
        """Write some data to storage and read it back."""
        self.system = System(self.env,
                             storage_config={
                                 "write_delay": 1,
                                 "read_delay": 1
                             })

        self.env.process(
            self.system.run_sequential_write_read(sequential_reads=1,
                                                  sequential_writes=1))
        self.env.run(until=4)
        result = self.system.get_result()
        self.assertEqual(result["writes"], 2)
        self.assertEqual(result["reads"], 2)

        pass
