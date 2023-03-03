from toyssd.bus import Bus, Path


class StorageBus(object):
    """Connects a host to storage so it can read and write data."""

    def __init__(self, env):
        self._env = env
        self._bus = Bus(env)
        self._to_storage = Path(self._bus)
        self._to_host = Path(self._bus)

    def start_transaction(self, payload):
        """Send a new transaction to storage."""
        self._to_storage.put(payload)

    def end_transaction(self, payload):
        """Send a completed transaction to host."""
        self._to_host.put(payload)

    def get_payloads_for_storage(self):
        """Get payloads for storage."""
        return self._to_storage.get_items()

    def get_payloads_for_host(self):
        """Get payloads for host."""
        return self._to_host.get_items()

    def get_transfer_latency(self):
        """Get the transfer latency."""
        return self._bus.get_transfer_latency()

    def get_payload_for_storage(self):
        """Pop payloads for storage."""
        return self._to_storage.get()
