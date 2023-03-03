"""Test for the host module."""
import unittest
from unittest.mock import MagicMock
import simpy

from toyssd.host import Host
from toyssd.storage_bus import StorageBus


class TestHost(unittest.TestCase):

    def setUp(self):
        self.env = simpy.Environment()
        self.host = Host(self.env)
        self.storage_bus = StorageBus(self.env)
        self.host.set_storage_bus(self.storage_bus)

    def test_create_host(self):
        self.assertIsInstance(self.host, Host)
        pass

    def test_write(self):
        """Write some data to storage."""
        self.storage_bus.start_transaction = MagicMock()
        self.host.write(0)
        self.storage_bus.start_transaction.assert_called_once()

        pass