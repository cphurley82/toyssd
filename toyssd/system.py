import simpy

from toyssd.storage import Storage


class System(object):
    """A system has a host which read and writes to a storage device over a 
    bus.
    """

    def __init__(self, env):
        self.env = env
        self.storage_resource = simpy.Resource(env, capacity=1)
        self.operations_completed = 0
        self.next_operation_id = 0
        self.write_number = 0
        self.read_number = 0
        self.drive = Storage(env, 0)

    def run_sequential_write_read(self,
                                  sequential_writes=1,
                                  sequential_reads=1):

        while True:
            for _ in range(sequential_writes):
                yield self.env.process(
                    self.writer(id=self.next_operation_id,
                                address=self.write_number))
                self.next_operation_id += 1
                self.write_number += 1

            for _ in range(sequential_reads):
                yield self.env.process(
                    self.reader(self.next_operation_id, self.read_number))
                self.next_operation_id += 1
                self.read_number += 1

    def writer(self, id, address):
        self.operations_completed += 1
        with self.storage_resource.request() as req:
            yield req

            print(f'id={id} starting write at {self.env.now}')
            yield self.env.process(
                self.drive.write(address=address,
                                 data=f'id={id} address={address}'))
            print(f'id={id} ending write at {self.env.now}')

    def reader(self, id, address):
        self.operations_completed += 1
        with self.storage_resource.request() as req:
            yield req

            print(f'id={id} starting read at {self.env.now}')
            data = yield self.env.process(self.drive.read(address))
            print(f'id={id} ending read at {self.env.now}, data={data}')

    def get_result(self):
        return {"writes": self.write_number, "reads": self.read_number}


if __name__ == '__main__':
    env = simpy.Environment()
    system = System(env)
    env.process(
        system.run_sequential_write_read(sequential_writes=2,
                                         sequential_reads=3))
    env.run(until=10)
    operations_per_second = system.operations_completed / env.now
    print(
        f'time={env.now} operations_completed={system.operations_completed} operations_per_second={operations_per_second}'
    )
    print('done')