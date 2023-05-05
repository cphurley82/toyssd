import simpy

from toyssd.host import Host
from toyssd.storage_bus import StorageBus
from toyssd.storage import Storage


class Result(object):
    """Information collected from a simulation used for analysis.
    
    Attributes:
        writes: Number of writes performed.
        reads: Number of reads performed.
        elapsed_time: Time it took to perform the writes and reads.
    """

    def __init__(self, writes=0, reads=0, elapsed_time=0):
        """Initialize the result with the given values.
        
        Args:
            writes: Initial number of writes performed.
            reads: Initial number of reads performed.
            elapsed_time: Initial value for elapsed time.
        """
        self.writes = writes
        self.reads = reads
        self.elapsed_time = elapsed_time


class System(object):
    """Simulates workloads on a storage system and collects results."""

    def __init__(self):
        """Initialize the system and its simulation environment."""
        self.env = simpy.Environment()
        self.storage_bus = StorageBus(self.env)
        self.host = Host(self.env)
        self.storage = Storage(self.env)

        # Connect the host and storage to the communication bus.
        self.host.set_storage_bus(self.storage_bus)
        self.storage.set_storage_bus(self.storage_bus)

    def run_write_read_workload(self, num_addresses):
        """Write, read, and repeat for the given number of addresses.
        
        Args:
            num_addresses: Number of addresses to write and read.

        Returns:
            Result: Information collected from the simulation.
        """
        result = Result()
        for address in range(num_addresses):
            print(
                f'[{self.env.now}] System writing address {address} through host.'
            )
            yield self.host.write(address)
            result.writes += 1
            self.env.run()
            print(
                f'[{self.env.now}] System reading address {address} through host.'
            )
            yield self.host.read(address)
            result.reads += 1
            self.env.run()
        result.elapsed_time = self.env.now
        return result