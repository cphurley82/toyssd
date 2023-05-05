import simpy


class Path(object):
    """A path defines a one-way route between two devices through the bus."""

    def __init__(self, bus):
        self.bus = bus
        self.store = simpy.Store(bus.env, capacity=1)

    def put(self, value):
        self.bus.put(value, self.store)

    def get(self):
        return self.bus.get(self.store)

    def get_items(self):
        return self.store.items


class Bus(object):
    """A bus provides a shared path between devices and has a latency."""

    def __init__(self, env):
        self.env = env
        self.transfer_latency = 1
        self.lock = simpy.Resource(env, capacity=1)

    def get_transfer_latency(self):
        return self.transfer_latency

    def delay(self, value, store):
        with self.lock.request() as req:
            yield req
            yield self.env.timeout(self.get_transfer_latency())
            store.put(value)

    def put(self, value, store):
        self.env.process(self.delay(value, store))

    def get(self, store):
        return store.get()