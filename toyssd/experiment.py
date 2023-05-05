import simpy


class System(object):
    """A system has a host which read and writes to a storage device over a 
    bus.
    """

    def __init__(self, env):
        self.env = env
        self.storage_resource = simpy.Resource(env, capacity=2)
        self.operation_number = 0

        # Start the run process every time an instance is created.
        # self.action = env.process(self.run())

    def run(self):
        for _ in range(4):
            self.env.process(self.writer(self.operations))
            self.env.process(self.reader(self.operations))

    def writer(self):
        self.operation_number += 1
        with self.storage_resource.request() as req:
            yield req

            print(f'id={self.operation_number} starting write at {env.now}')
            yield self.env.process(self.write())
            print(f'id={self.operation_number} ending write at {env.now}')

    def reader(self,):
        self.operations += 1
        with self.storage_resource.request() as req:
            yield req

            print(f'id={self.operation_number} starting read at {env.now}')
            yield self.env.process(self.read())
            print(f'id={self.operation_number} ending read at {env.now}')

    def write(self):
        yield self.env.timeout(1)

    def read(self):
        yield self.env.timeout(2)


if __name__ == '__main__':
    env = simpy.Environment()
    system = System(env)
    system.run()
    env.run()
    operations_per_second = system.operations / env.now
    print(f'operations_per_second={operations_per_second}')
    print('done')