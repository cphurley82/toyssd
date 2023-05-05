import simpy


class Host(object):

    def __init__(self, env, name, drive, data):
        self.env = env
        self.name = name
        self.drive = drive
        self.data = data
        self.action = env.process(self.run())

    def run(self):
        address = 0
        while True:
            write_data = f'{self.name} data{address}'
            with self.drive.request() as req:
                try:
                    yield req
                    yield self.env.process(self.write_delay())
                    self.write(address, write_data)
                    print(
                        f'[{self.env.now}] {self.name} finished writing to {address}.'
                    )
                except simpy.Interrupt:
                    # When we received an interrupt, we stop the write process and
                    # continue.
                    print(
                        f'[{self.env.now}] {self.name} write process aborted.')

            with self.drive.request() as req:
                try:
                    yield req
                    yield self.env.process(self.read_delay())
                    read_data = self.read(address)
                    print(
                        f'[{self.env.now}] {self.name} finished reading {read_data} from {address}.'
                    )
                except simpy.Interrupt:
                    # When we received an interrupt, we stop the read process and
                    # continue.
                    print(
                        f'[{self.env.now}] {self.name} read {address} process aborted.'
                    )
            address += 1

    def write_delay(self):
        write_duration = 5
        yield self.env.timeout(write_duration)

    def write(self, address, data):
        self.data[address] = data
        print(f'[{self.env.now}] {self.name} wrote {data} to {address}.')

    def read_delay(self):
        read_duration = 10
        yield self.env.timeout(read_duration)

    def read(self, address):
        data = None
        if address in self.data:
            data = self.data[address]
        print(f'[{self.env.now}] {self.name} read {data} from {address}.')
        return data


def host_abort(env, host):
    yield env.timeout(61)
    host.action.interrupt()


if __name__ == '__main__':
    env = simpy.Environment()
    drive = simpy.Resource(env, capacity=1)
    data = {}
    hosts = []
    for i in range(2):
        hosts.append(Host(env, f'host{i}', drive, data))
    env.process(host_abort(env, hosts[0]))
    for i in range(1, 101):
        env.run(until=i)
        print('.', end='')
    print(data)