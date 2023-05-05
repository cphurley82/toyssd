import simpy
from toyssd.storage import Payload, Command


class Host(object):
    """A host reads and writes to a storage device."""

    def __init__(self, env):
        self.env = env
        self.storage_bus = None
        self.env.process(self.receiver())

    def set_storage_bus(self, bus):
        self.storage_bus = bus

    def write(self, address):
        """Issue write transaction."""
        payload = Payload(Command.WRITE, address)
        print(
            f'[{self.env.now}] Host starting transaction with payload {payload}.'
        )
        self.storage_bus.start_transaction(payload)

    def read(self, address):
        """Issue read transaction."""
        payload = Payload(Command.READ, address)
        print(
            f'[{self.env.now}] Host starting transaction with payload {payload}.'
        )
        return self.storage_bus.start_transaction(payload)

    def receiver(self):
        while True:
            payload = yield self.storage_bus.get_payload_event_received_by_host(
            )
            print(
                f'[{self.env.now}] Host received payload from storage {payload}.'
            )
