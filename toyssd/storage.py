import simpy
from enum import Enum


class Command(Enum):
    """A command is a type of message that can be executed by Storage."""
    READ = 1
    WRITE = 2


class Payload(object):
    """A payload is a message used to communicate with Storage."""

    def __init__(self, command, address=None, data=None):
        self.command = command
        self.address = address
        self.data = data

    def __repr__(self):
        return f"Payload(command={self.command} address={self.address}, data={self.data})"


class Storage(object):
    """A storage device can be read from and written to."""

    def __init__(self, env):
        self.env = env
        self.data = {}
        self.storage_bus = None
        self.process_lock = simpy.Resource(env, capacity=1)

    def set_storage_bus(self, bus):
        self.storage_bus = bus
        self.env.process(self.process_events())

    def evaluate(self, payload):
        """Evaluate a payload."""
        if payload.command == Command.READ:
            payload.data = self.data[payload.address]
        elif payload.command == Command.WRITE:
            self.data[payload.address] = payload.data
        else:
            raise ValueError(f"Unknown command: {payload.command}")

        return payload

    def process_events(self):
        while True:
            payload = yield self.storage_bus.get_payload_event_received_by_storage(
            )
            with self.process_lock.request() as request:
                yield request
                print(
                    f'[{self.env.now}] Storage received payload {payload} and will evaluate.'
                )
                self.evaluate(payload)
                self.storage_bus.end_transaction(payload)