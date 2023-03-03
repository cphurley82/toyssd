import unittest
import simpy

from toyssd.storage_bus import StorageBus


class TestStorageBus(unittest.TestCase):
    """Tests for the storage interface module."""

    def setUp(self):
        self.env = simpy.Environment()
        self.storage_bus = StorageBus(self.env)

    def test_start_transaction(self):
        """Write some data to storage and read it back."""
        self.storage_bus.start_transaction(1)
        self.env.run()
        self.assertEqual(self.storage_bus.get_payloads_for_storage(), [1])

        # Simulation time should have moved forward based on the transfer delay.
        self.assertEqual(self.env.now,
                         self.storage_bus.get_transfer_latency() * 1)
        pass

    def test_end_transaction(self):
        """Write some data to storage and read it back."""
        self.storage_bus.end_transaction(1)
        self.env.run()
        self.assertEqual(self.storage_bus.get_payloads_for_host(), [1])

        # Simulation time should have moved forward based on the transfer delay.
        self.assertEqual(self.env.now,
                         self.storage_bus.get_transfer_latency() * 1)
        pass