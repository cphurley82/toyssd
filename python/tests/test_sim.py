"""Comprehensive unit tests for the sim module.

Test Coverage Goals:
- Test all exception types and their usage contexts
- Test ToySSD initialization with various configurations
- Test workload execution and event recording
- Test stats tracking and write amplification calculation
- Test bridge protocol implementations (_InMemoryBridge)
- Test error conditions and edge cases
"""

import pytest

from toyssd import NandGeometry, SimConfig, ToySSD, Workload
from toyssd.sim import (
    CapacityError,
    CommandError,
    HardwareError,
    SimulationError,
    ToySSDException,
    ToySSDStats,
)


class TestExceptions:
    """Test suite for ToySSD exception hierarchy.
    
    These tests verify that exceptions are properly defined and can be
    raised/caught as expected. The exception hierarchy allows users to
    catch specific errors or all ToySSD errors via the base class.
    """

    def test_toyssd_exception_is_base(self) -> None:
        """Verify ToySSDException is a base exception for simulator errors."""
        exc = ToySSDException("test message")
        assert isinstance(exc, Exception)
        assert str(exc) == "test message"

    def test_simulation_error_inherits_base(self) -> None:
        """Verify SimulationError inherits from ToySSDException."""
        exc = SimulationError("simulation failed")
        assert isinstance(exc, ToySSDException)
        assert isinstance(exc, Exception)
        assert str(exc) == "simulation failed"

    def test_command_error_inherits_base(self) -> None:
        """Verify CommandError inherits from ToySSDException."""
        exc = CommandError("invalid command")
        assert isinstance(exc, ToySSDException)
        assert str(exc) == "invalid command"

    def test_capacity_error_inherits_base(self) -> None:
        """Verify CapacityError inherits from ToySSDException."""
        exc = CapacityError("capacity exceeded")
        assert isinstance(exc, ToySSDException)
        assert str(exc) == "capacity exceeded"

    def test_hardware_error_inherits_base(self) -> None:
        """Verify HardwareError inherits from ToySSDException."""
        exc = HardwareError("nand failure")
        assert isinstance(exc, ToySSDException)
        assert str(exc) == "nand failure"

    def test_catch_all_toyssd_exceptions(self) -> None:
        """Verify all ToySSD exceptions can be caught via base class."""
        exceptions = [
            SimulationError("sim"),
            CommandError("cmd"),
            CapacityError("cap"),
            HardwareError("hw"),
        ]
        for exc in exceptions:
            try:
                raise exc
            except ToySSDException as caught:
                assert caught is exc


class TestToySSDStats:
    """Test suite for ToySSDStats data class.
    
    These tests verify stats tracking and derived metrics like write
    amplification. Stats are central to evaluating simulator behavior.
    """

    def test_default_stats(self) -> None:
        """Verify default stats start at zero."""
        stats = ToySSDStats()
        assert stats.total_writes == 0
        assert stats.total_reads == 0
        assert stats.total_erases == 0

    def test_stats_with_custom_values(self) -> None:
        """Verify stats can be initialized with custom values."""
        stats = ToySSDStats(total_writes=10, total_reads=20, total_erases=5)
        assert stats.total_writes == 10
        assert stats.total_reads == 20
        assert stats.total_erases == 5

    def test_write_amplification_zero_writes(self) -> None:
        """Verify write_amplification returns 0.0 when no writes occurred."""
        stats = ToySSDStats(total_writes=0)
        assert stats.write_amplification == 0.0

    def test_write_amplification_with_writes(self) -> None:
        """Verify write_amplification calculation (currently 1.0 for direct map)."""
        stats = ToySSDStats(total_writes=100)
        # Direct map skeleton keeps amplification at 1.0
        assert stats.write_amplification == 1.0

    def test_stats_is_mutable(self) -> None:
        """Verify ToySSDStats is mutable for runtime updates."""
        stats = ToySSDStats()
        stats.total_writes = 50
        stats.total_reads = 30
        stats.total_erases = 10
        assert stats.total_writes == 50
        assert stats.total_reads == 30
        assert stats.total_erases == 10


class TestToySSDInitialization:
    """Test suite for ToySSD initialization and configuration.
    
    These tests verify that ToySSD properly initializes with various
    configurations and correctly selects backends.
    """

    def test_init_with_python_backend(self) -> None:
        """Verify ToySSD initializes successfully with python backend."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, backend="python")
        sim = ToySSD(config)
        assert sim._config == config
        assert sim._stats.total_writes == 0
        assert sim._stats.total_reads == 0

    def test_init_with_systemc_backend_raises_error(self) -> None:
        """Verify SystemC backend raises clear error when unavailable."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, backend="systemc")
        with pytest.raises(
            SimulationError,
            match="SystemC backend requested but native bindings are not yet available",
        ):
            ToySSD(config)

    def test_init_with_visualization_enabled(self) -> None:
        """Verify visualization is created when enabled."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, enable_visualization=True)
        sim = ToySSD(config)
        assert sim.viz is not None

    def test_init_with_visualization_disabled(self) -> None:
        """Verify visualization is not created when disabled."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, enable_visualization=False)
        sim = ToySSD(config)
        assert sim.viz is None


class TestToySSDWorkloadExecution:
    """Test suite for workload execution and event handling.
    
    These tests verify that ToySSD correctly processes workloads,
    records events, updates stats, and manages the visualization.
    """

    def test_run_workload_write(self) -> None:
        """Verify run_workload executes a write workload and updates stats."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, enable_visualization=False)
        sim = ToySSD(config)
        
        workload = Workload.sequential_write(
            start_lba=0, length_gb=0.0001, block_size_kb=4
        )
        sim.run_workload(workload)
        
        stats = sim.get_stats()
        assert stats.total_writes > 0

    def test_run_workload_read(self) -> None:
        """Verify run_workload executes a read workload after write."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, enable_visualization=False)
        sim = ToySSD(config)
        
        # Write first
        write = Workload.sequential_write(
            start_lba=0, length_gb=0.0001, block_size_kb=4
        )
        sim.run_workload(write)
        
        # Then read
        read = Workload.sequential_read(
            start_lba=0, length_gb=0.0001, block_size_kb=4
        )
        sim.run_workload(read)
        
        stats = sim.get_stats()
        assert stats.total_reads > 0

    def test_run_workload_with_duration(self) -> None:
        """Verify run_workload accepts optional duration parameter."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, enable_visualization=False)
        sim = ToySSD(config)
        
        workload = Workload.sequential_write(
            start_lba=0, length_gb=0.0001, block_size_kb=4
        )
        # Should not raise an error
        sim.run_workload(workload, duration_ms=100)

    def test_submit_io_queues_workload(self) -> None:
        """Verify submit_io queues a workload without immediate execution."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, enable_visualization=False)
        sim = ToySSD(config)
        
        workload = Workload.sequential_write(
            start_lba=0, length_gb=0.0001, block_size_kb=4
        )
        # submit_io queues but doesn't execute immediately
        sim.submit_io(workload)
        
        # Should record a queue event
        events = sim.drain_events()
        assert len(events) > 0
        assert any(e.get("type") == "queue" for e in events)

    def test_step_advances_simulation(self) -> None:
        """Verify step advances simulation time."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, enable_visualization=False)
        sim = ToySSD(config)
        
        # Step without any pending work
        sim.step(100)
        
        events = sim.drain_events()
        # Should have idle event when no work pending
        assert any(e.get("type") == "idle" for e in events)

    def test_step_with_negative_duration_raises_error(self) -> None:
        """Verify step raises ValueError for negative duration."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, enable_visualization=False)
        sim = ToySSD(config)
        
        with pytest.raises(ValueError, match="duration_ms must be positive"):
            sim.step(-10)

    def test_step_with_zero_duration_raises_error(self) -> None:
        """Verify step raises ValueError for zero duration."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, enable_visualization=False)
        sim = ToySSD(config)
        
        with pytest.raises(ValueError, match="duration_ms must be positive"):
            sim.step(0)

    def test_drain_events_returns_and_clears(self) -> None:
        """Verify drain_events returns events and clears the queue."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, enable_visualization=False)
        sim = ToySSD(config)
        
        workload = Workload.sequential_write(
            start_lba=0, length_gb=0.0001, block_size_kb=4
        )
        sim.run_workload(workload)
        
        # First drain should return events
        events1 = sim.drain_events()
        assert len(events1) > 0
        
        # Second drain should return empty list
        events2 = sim.drain_events()
        assert len(events2) == 0

    def test_get_stats_returns_current_stats(self) -> None:
        """Verify get_stats returns the current stats object."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, enable_visualization=False)
        sim = ToySSD(config)
        
        stats1 = sim.get_stats()
        assert stats1.total_writes == 0
        
        workload = Workload.sequential_write(
            start_lba=0, length_gb=0.0001, block_size_kb=4
        )
        sim.run_workload(workload)
        
        stats2 = sim.get_stats()
        assert stats2.total_writes > 0

    def test_shutdown_clears_state(self) -> None:
        """Verify shutdown gracefully tears down simulation."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, enable_visualization=False)
        sim = ToySSD(config)
        
        workload = Workload.sequential_write(
            start_lba=0, length_gb=0.0001, block_size_kb=4
        )
        sim.run_workload(workload)
        
        # Should not raise an error
        sim.shutdown()

    def test_visualization_updates_on_events(self) -> None:
        """Verify visualization receives events when enabled."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, enable_visualization=True)
        sim = ToySSD(config)
        
        workload = Workload.sequential_write(
            start_lba=0, length_gb=0.0001, block_size_kb=4
        )
        sim.run_workload(workload)
        
        assert sim.viz is not None
        # Visualization should have recorded events
        history = sim.viz.history()
        assert len(history) > 0


class TestInMemoryBridgeWorkloads:
    """Test suite for _InMemoryBridge workload processing.
    
    These tests verify the in-memory bridge correctly handles various
    workload patterns and error conditions.
    """

    def test_read_unwritten_lba_raises_capacity_error(self) -> None:
        """Verify reading unwritten LBA raises CapacityError."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, enable_visualization=False)
        sim = ToySSD(config)
        
        # Try to read before writing
        read = Workload.sequential_read(
            start_lba=0, length_gb=0.0001, block_size_kb=4
        )
        
        with pytest.raises(CapacityError, match="LBA .* not written before read"):
            sim.run_workload(read)

    def test_write_then_read_verifies_data(self) -> None:
        """Verify write-then-read verifies data pattern correctly."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, enable_visualization=False)
        sim = ToySSD(config)
        
        # Write pattern
        write = Workload.sequential_write(
            start_lba=10, length_gb=0.0001, block_size_kb=4
        )
        sim.run_workload(write)
        
        # Read should verify the pattern
        read = Workload.sequential_read(
            start_lba=10, length_gb=0.0001, block_size_kb=4
        )
        sim.run_workload(read)  # Should not raise

    def test_random_write_workload(self) -> None:
        """Verify random write workload executes correctly."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, enable_visualization=False)
        sim = ToySSD(config)
        
        workload = Workload.random_write(
            lba_range=(0, 100),
            io_count=10,
            block_size_kb=4,
            randomness_seed=42,
        )
        sim.run_workload(workload)
        
        stats = sim.get_stats()
        assert stats.total_writes >= 10

    def test_random_read_workload_after_write(self) -> None:
        """Verify random read workload executes after writes."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, enable_visualization=False)
        sim = ToySSD(config)
        
        # Write first
        write = Workload.random_write(
            lba_range=(0, 100),
            io_count=50,
            block_size_kb=4,
            randomness_seed=42,
        )
        sim.run_workload(write)
        
        # Read with same seed
        read = Workload.random_read(
            lba_range=(0, 50),
            io_count=10,
            block_size_kb=4,
            randomness_seed=42,
        )
        sim.run_workload(read)
        
        stats = sim.get_stats()
        assert stats.total_reads >= 10

    def test_invalid_block_size_raises_command_error(self) -> None:
        """Verify invalid block size raises CommandError."""
        from toyssd.workload import WorkloadKind
        
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, enable_visualization=False)
        sim = ToySSD(config)
        
        # Create workload with invalid block size
        workload = Workload(
            kind=WorkloadKind.SEQUENTIAL_WRITE,
            start_lba=0,
            lba_count=1,
            block_size_kb=0,  # Invalid
            queue_depth=1,
        )
        
        with pytest.raises(CommandError, match="block_size_kb must be positive"):
            sim.run_workload(workload)

    def test_step_processes_pending_workload(self) -> None:
        """Verify step processes pending workloads."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, enable_visualization=False)
        sim = ToySSD(config)
        
        # Queue a workload
        workload = Workload.sequential_write(
            start_lba=0, length_gb=0.0001, block_size_kb=4
        )
        sim.submit_io(workload)
        
        # Clear queue event
        sim.drain_events()
        
        # Step should process the pending workload
        sim.step(100)
        
        stats = sim.get_stats()
        assert stats.total_writes > 0


class TestEventRecording:
    """Test suite for event recording and stats updates.
    
    These tests verify that events are correctly recorded and stats
    are properly updated based on event types.
    """

    def test_write_event_increments_write_stats(self) -> None:
        """Verify write events increment total_writes."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, enable_visualization=False)
        sim = ToySSD(config)
        
        initial_writes = sim.get_stats().total_writes
        
        workload = Workload.sequential_write(
            start_lba=0, length_gb=0.0001, block_size_kb=4
        )
        sim.run_workload(workload)
        
        final_writes = sim.get_stats().total_writes
        assert final_writes > initial_writes

    def test_read_event_increments_read_stats(self) -> None:
        """Verify read events increment total_reads."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, enable_visualization=False)
        sim = ToySSD(config)
        
        # Write first
        write = Workload.sequential_write(
            start_lba=0, length_gb=0.0001, block_size_kb=4
        )
        sim.run_workload(write)
        
        initial_reads = sim.get_stats().total_reads
        
        # Then read
        read = Workload.sequential_read(
            start_lba=0, length_gb=0.0001, block_size_kb=4
        )
        sim.run_workload(read)
        
        final_reads = sim.get_stats().total_reads
        assert final_reads > initial_reads

    def test_multiple_workloads_accumulate_stats(self) -> None:
        """Verify multiple workloads accumulate stats correctly."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, enable_visualization=False)
        sim = ToySSD(config)
        
        # Multiple writes
        for i in range(3):
            workload = Workload.sequential_write(
                start_lba=i * 10, length_gb=0.0001, block_size_kb=4
            )
            sim.run_workload(workload)
        
        stats = sim.get_stats()
        # Should have accumulated writes from all workloads
        assert stats.total_writes > 3

    def test_erase_event_increments_erase_stats(self) -> None:
        """Verify erase events increment total_erases."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, enable_visualization=False)
        sim = ToySSD(config)
        
        # Manually inject an erase event to test the erase path
        erase_events = [{"type": "erase", "lba": 100}]
        sim._record_events(erase_events)
        
        stats = sim.get_stats()
        assert stats.total_erases == 1
        
        # Verify event was recorded
        events = sim.drain_events()
        assert len(events) == 1
        assert events[0]["type"] == "erase"
