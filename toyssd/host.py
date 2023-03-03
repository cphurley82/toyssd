from toyssd.storage import Payload, Command


class Host(object):
    """A host reads and writes to a storage device."""

    def __init__(self, env):
        self.env = env
        self.storage_bus = None

    def set_storage_bus(self, bus):
        self.storage_bus = bus

    def write(self, address):
        """Issue write transaction."""
        payload = Payload(Command.WRITE, address)
        self.storage_bus.start_transaction(payload)
        pass

    def read(self, address):
        """Issue read transaction."""
        payload = Payload(Command.READ, address)
        self.storage_bus.start_transaction(payload)
        pass

    def receiver(self, payload):
        while True:
            yield self.storage_bus.get_payloads_for_host()