import simpy

from toyssd.host import Host
from toyssd.storage_bus import StorageBus
from toyssd.storage import Storage


class Result(object):
    """A result is a value that is returned from a simulation."""

    def __init__(self, writes=0, reads=0, elapsed_time=0):
        self.writes = writes
        self.reads = reads
        self.elapsed_time = elapsed_time


class System(object):
    """A system has a host which read and writes to a storage device over a 
    bus.
    """

    def __init__(self):
        self.env = simpy.Environment()
        self.storage_bus = StorageBus(self.env)
        self.host = Host(self.env)
        self.storage = Storage(self.env)

        self.host.set_storage_bus(self.storage_bus)
        self.storage.set_storage_bus(self.storage_bus)

    def run_write_read_workload(self, num_addresses):
        """Run a workload that writes and reads data to storage."""
        result = Result()
        for address in range(num_addresses):
            self.host.write(address)
            result.writes += 1
            self.host.read(address)
            result.reads += 1
        self.env.run()
        result.elapsed_time = self.env.now
        return result