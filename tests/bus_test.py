import unittest
import simpy

from toyssd.bus import Bus, Path


class TestBus(unittest.TestCase):
    """Tests for the bus module."""

    def setUp(self):
        self.env = simpy.Environment()
        self.bus = Bus(self.env)
        self.to_storage = Path(self.bus)
        self.to_host = Path(self.bus)

    def test_put(self):
        """Put stuff on the bus and make sure it gets to the other side."""
        self.to_storage.put(1)
        self.to_storage.put(2)
        self.to_host.put(3)
        self.to_host.put(4)
        self.to_storage.put(5)
        self.to_host.put(6)

        self.env.run()

        self.assertEqual(self.to_storage.get_items(), [1, 2, 5])
        self.assertEqual(self.to_host.get_items(), [3, 4, 6])

        # Simulation time should have moved forward based on the transfer delay.
        self.assertEqual(self.env.now, self.bus.get_transfer_latency() * 6)
        pass

    def test_get(self):
        """Put stuff on the bus and make sure you can get it out."""
        to_storage_values = [1, 2, 5]
        to_host_values = [3, 4, 6]

        self.to_storage.put(1)
        self.to_storage.put(2)
        self.to_host.put(3)
        self.to_host.put(4)
        self.to_storage.put(5)
        self.to_host.put(6)

        def receiver(path, expected_values):
            for expected_value in expected_values:
                value = yield path.get()
                self.assertEqual(value, expected_value)

        self.env.process(receiver(self.to_storage, to_storage_values))
        self.env.process(receiver(self.to_host, to_host_values))
        self.env.run()

        # Both queues should be empty after getting everything out.
        self.assertEqual(self.to_storage.get_items(), [])
        self.assertEqual(self.to_host.get_items(), [])

        # Simulation time should have moved forward based on the transfer delay.
        self.assertEqual(self.env.now, self.bus.get_transfer_latency() * 6)
