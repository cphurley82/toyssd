import simpy


class Drive(object):

    def __init__(self, env, id):
        self.env = env
        self.id = id
        self.write_delay = 1
        self.read_delay = 2
        self.data = {}

    def write(self, address, data):
        yield self.env.timeout(self.write_delay)
        self.data[address] = data

    def read(self, address):
        yield self.env.timeout(self.read_delay)
        try:
            data = self.data[address]
        except KeyError:
            data = None
        return data
