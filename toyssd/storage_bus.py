from toyssd.bus import Bus, Path


class StorageBus(object):
    """Connects a host to storage so they can communicate and transfer data.
    
    Simulates latency for transferring information between host
    and storage and also the shared nature of the bus. After traversing the
    bus, payloads are retrieved in FIFO order.

    TODO(cphurley): Verify data read from storage is correct.
    """

    def __init__(self, env):
        """Initialize the storage bus in the given environment.
        
        Args:
            env: SimPy environment.
        """
        self._env = env
        self._bus = Bus(env)
        self._to_storage = Path(self._bus)
        self._to_host = Path(self._bus)

    def start_transaction(self, payload):
        """Send a transaction payload to storage.
        
        Args:
            payload: Payload to send to storage.
        """
        print(
            f'[{self._env.now}] Storage bus sending payload {payload} to storage.'
        )
        return self._to_storage.put(payload)

    def end_transaction(self, payload):
        """Send a completed transaction payload back to host.
        
        Args:
            payload: Payload to send to host.
        """
        print(
            f'[{self._env.now}] Storage bus sending payload {payload} to host.')
        self._to_host.put(payload)

    def get_payloads_for_storage(self):
        """Returns list of payloads sent to storage after traversing the bus."""
        return self._to_storage.get_items()

    def get_payloads_for_host(self):
        """Returns list of payloads sent to host after traversing the bus."""
        return self._to_host.get_items()

    def get_transfer_latency(self):
        """Returns the transfer latency time."""
        return self._bus.get_transfer_latency()

    def get_payload_event_received_by_storage(self):
        """Returns oldest payload sent to storage, removing it from the bus."""
        payload_received_event = self._to_storage.get()
        print(
            f'[{self._env.now}] Storage bus event for storage {payload_received_event} retrieved.'
        )
        return payload_received_event

    def get_payload_event_received_by_host(self):
        """Returns oldest payload sent to host, removing it from the bus."""
        payload_received_event = self._to_host.get()
        print(
            f'[{self._env.now}] Storage bus event for host {payload_received_event} retrieved.'
        )
        return payload_received_event