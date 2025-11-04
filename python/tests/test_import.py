"""Basic smoke tests for the Python package."""

from toyssd import NandGeometry, SimConfig, ToySSD, Workload


def test_sequential_write_and_read_roundtrip() -> None:
    geometry = NandGeometry()
    config = SimConfig(nand_geometry=geometry, enable_visualization=False)
    sim = ToySSD(config)

    write = Workload.sequential_write(
        start_lba=0,
        length_gb=0.0001,
        block_size_kb=4,
    )
    sim.run_workload(write)

    read = Workload.sequential_read(
        start_lba=0,
        length_gb=0.0001,
        block_size_kb=4,
    )
    sim.run_workload(read)

    stats = sim.get_stats()
    assert stats.total_writes > 0
    assert stats.total_reads > 0
