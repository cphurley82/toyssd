import unittest
from unittest.mock import MagicMock
import simpy

from toyssd.storage import Storage, Payload, Command
from toyssd.storage_bus import StorageBus


class TestStorage(unittest.TestCase):

    def setUp(self):
        self.env = simpy.Environment()
        self.storage = Storage(self.env)
        self.storage_bus = StorageBus(self.env)
        self.storage.set_storage_bus(self.storage_bus)

    def test_create_storage(self):
        self.assertIsInstance(self.storage, Storage)
        pass

    def test_write_read(self):
        """Write some data to storage and read it back."""
        write_payload = Payload(Command.WRITE, 0, 'hello world')
        self.storage.evaluate(write_payload)
        read_payload = Payload(Command.READ, 0)
        self.storage.evaluate(read_payload)
        self.assertEqual(read_payload.data, write_payload.data)
        pass
