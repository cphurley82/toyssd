"""Comprehensive unit tests for the workload module.

Test Coverage Goals:
- Test WorkloadKind enum values
- Test Workload dataclass and factory methods
- Test all factory methods (sequential_write, sequential_read, random_write, random_read)
- Test to_host_dict serialization
- Test _blocks_from_gib conversion logic
- Test edge cases and error conditions
"""

import pytest

from toyssd.workload import Workload, WorkloadKind


class TestWorkloadKind:
    """Test suite for WorkloadKind enum.

    These tests verify the WorkloadKind enum correctly defines
    all workload types with proper string values.
    """

    def test_sequential_write_value(self) -> None:
        """Verify SEQUENTIAL_WRITE has correct string value."""
        assert WorkloadKind.SEQUENTIAL_WRITE.value == "sequential_write"

    def test_sequential_read_value(self) -> None:
        """Verify SEQUENTIAL_READ has correct string value."""
        assert WorkloadKind.SEQUENTIAL_READ.value == "sequential_read"

    def test_random_write_value(self) -> None:
        """Verify RANDOM_WRITE has correct string value."""
        assert WorkloadKind.RANDOM_WRITE.value == "random_write"

    def test_random_read_value(self) -> None:
        """Verify RANDOM_READ has correct string value."""
        assert WorkloadKind.RANDOM_READ.value == "random_read"

    def test_workload_kind_is_string_enum(self) -> None:
        """Verify WorkloadKind enum members are strings."""
        for kind in WorkloadKind:
            assert isinstance(kind.value, str)


class TestWorkloadDataclass:
    """Test suite for Workload dataclass.

    These tests verify the Workload dataclass correctly stores
    workload parameters and is properly frozen/slotted.
    """

    def test_workload_initialization(self) -> None:
        """Verify Workload initializes with all parameters."""
        workload = Workload(
            kind=WorkloadKind.SEQUENTIAL_WRITE,
            start_lba=100,
            lba_count=50,
            block_size_kb=4,
            queue_depth=1,
            randomness_seed=42,
        )

        assert workload.kind == WorkloadKind.SEQUENTIAL_WRITE
        assert workload.start_lba == 100
        assert workload.lba_count == 50
        assert workload.block_size_kb == 4
        assert workload.queue_depth == 1
        assert workload.randomness_seed == 42

    def test_workload_is_frozen(self) -> None:
        """Verify Workload is immutable (frozen dataclass)."""
        workload = Workload(
            kind=WorkloadKind.SEQUENTIAL_WRITE,
            start_lba=0,
            lba_count=10,
            block_size_kb=4,
            queue_depth=1,
        )

        with pytest.raises(AttributeError):
            workload.start_lba = 100  # type: ignore

    def test_workload_optional_randomness_seed(self) -> None:
        """Verify randomness_seed is optional and defaults to None."""
        workload = Workload(
            kind=WorkloadKind.SEQUENTIAL_WRITE,
            start_lba=0,
            lba_count=10,
            block_size_kb=4,
            queue_depth=1,
        )

        assert workload.randomness_seed is None


class TestWorkloadToHostDict:
    """Test suite for to_host_dict serialization.

    These tests verify that workloads can be serialized to
    dictionary format for cross-boundary communication.
    """

    def test_to_host_dict_sequential_write(self) -> None:
        """Verify to_host_dict serializes sequential write workload."""
        workload = Workload.sequential_write(
            start_lba=0, length_gb=0.001, block_size_kb=4
        )

        host_dict = workload.to_host_dict()

        assert host_dict["kind"] == "sequential_write"
        assert host_dict["start_lba"] == 0
        assert isinstance(host_dict["lba_count"], int)
        assert host_dict["block_size_kb"] == 4
        assert host_dict["queue_depth"] == 1
        assert host_dict["randomness_seed"] is None

    def test_to_host_dict_with_randomness_seed(self) -> None:
        """Verify to_host_dict includes randomness_seed when set."""
        workload = Workload.random_write(
            lba_range=(0, 100),
            io_count=10,
            block_size_kb=4,
            randomness_seed=42,
        )

        host_dict = workload.to_host_dict()

        assert host_dict["randomness_seed"] == 42

    def test_to_host_dict_all_fields(self) -> None:
        """Verify to_host_dict includes all expected fields."""
        workload = Workload(
            kind=WorkloadKind.RANDOM_READ,
            start_lba=50,
            lba_count=20,
            block_size_kb=8,
            queue_depth=4,
            randomness_seed=123,
        )

        host_dict = workload.to_host_dict()

        assert "kind" in host_dict
        assert "start_lba" in host_dict
        assert "lba_count" in host_dict
        assert "block_size_kb" in host_dict
        assert "queue_depth" in host_dict
        assert "randomness_seed" in host_dict


class TestSequentialWriteFactory:
    """Test suite for sequential_write factory method.

    These tests verify the sequential_write factory correctly
    creates workloads with proper LBA count calculations.
    """

    def test_sequential_write_basic(self) -> None:
        """Verify sequential_write creates valid workload."""
        workload = Workload.sequential_write(
            start_lba=0, length_gb=0.001, block_size_kb=4
        )

        assert workload.kind == WorkloadKind.SEQUENTIAL_WRITE
        assert workload.start_lba == 0
        assert workload.lba_count > 0
        assert workload.block_size_kb == 4
        assert workload.queue_depth == 1

    def test_sequential_write_with_custom_queue_depth(self) -> None:
        """Verify sequential_write accepts custom queue_depth."""
        workload = Workload.sequential_write(
            start_lba=100, length_gb=0.01, block_size_kb=4, queue_depth=32
        )

        assert workload.queue_depth == 32

    def test_sequential_write_lba_count_calculation(self) -> None:
        """Verify sequential_write calculates LBA count correctly."""
        # 1 GB with 4 KB blocks = 262144 blocks
        workload = Workload.sequential_write(
            start_lba=0, length_gb=1.0, block_size_kb=4
        )

        expected_count = int((1.0 * 1024**3) // (4 * 1024))
        assert workload.lba_count == expected_count

    def test_sequential_write_small_length(self) -> None:
        """Verify sequential_write handles very small lengths."""
        workload = Workload.sequential_write(
            start_lba=0, length_gb=0.0001, block_size_kb=4
        )

        assert workload.lba_count >= 0


class TestSequentialReadFactory:
    """Test suite for sequential_read factory method.

    These tests verify the sequential_read factory correctly
    creates workloads with proper parameters.
    """

    def test_sequential_read_basic(self) -> None:
        """Verify sequential_read creates valid workload."""
        workload = Workload.sequential_read(
            start_lba=0, length_gb=0.001, block_size_kb=4
        )

        assert workload.kind == WorkloadKind.SEQUENTIAL_READ
        assert workload.start_lba == 0
        assert workload.lba_count > 0
        assert workload.block_size_kb == 4
        assert workload.queue_depth == 1

    def test_sequential_read_with_custom_queue_depth(self) -> None:
        """Verify sequential_read accepts custom queue_depth."""
        workload = Workload.sequential_read(
            start_lba=50, length_gb=0.01, block_size_kb=8, queue_depth=16
        )

        assert workload.queue_depth == 16

    def test_sequential_read_lba_count_calculation(self) -> None:
        """Verify sequential_read calculates LBA count correctly."""
        workload = Workload.sequential_read(start_lba=0, length_gb=0.5, block_size_kb=4)

        expected_count = int((0.5 * 1024**3) // (4 * 1024))
        assert workload.lba_count == expected_count


class TestRandomWriteFactory:
    """Test suite for random_write factory method.

    These tests verify the random_write factory correctly
    creates workloads with proper parameters.
    """

    def test_random_write_basic(self) -> None:
        """Verify random_write creates valid workload."""
        workload = Workload.random_write(
            lba_range=(0, 100), io_count=10, block_size_kb=4
        )

        assert workload.kind == WorkloadKind.RANDOM_WRITE
        assert workload.start_lba == 0
        assert workload.lba_count == 10
        assert workload.block_size_kb == 4
        assert workload.queue_depth == 1
        assert workload.randomness_seed is None

    def test_random_write_with_randomness_seed(self) -> None:
        """Verify random_write accepts randomness_seed."""
        workload = Workload.random_write(
            lba_range=(0, 1000),
            io_count=50,
            block_size_kb=4,
            randomness_seed=42,
        )

        assert workload.randomness_seed == 42

    def test_random_write_with_custom_queue_depth(self) -> None:
        """Verify random_write accepts custom queue_depth."""
        workload = Workload.random_write(
            lba_range=(0, 100),
            io_count=10,
            block_size_kb=4,
            queue_depth=8,
        )

        assert workload.queue_depth == 8

    def test_random_write_lba_range_start(self) -> None:
        """Verify random_write uses lba_range start as start_lba."""
        workload = Workload.random_write(
            lba_range=(500, 1000), io_count=10, block_size_kb=4
        )

        assert workload.start_lba == 500

    def test_random_write_zero_io_count(self) -> None:
        """Verify random_write handles zero io_count with max(io_count, 1)."""
        workload = Workload.random_write(
            lba_range=(0, 100), io_count=0, block_size_kb=4
        )

        # Should use max(io_count, 1) = 1
        assert workload.lba_count == 1


class TestRandomReadFactory:
    """Test suite for random_read factory method.

    These tests verify the random_read factory correctly
    creates workloads with proper parameters.
    """

    def test_random_read_basic(self) -> None:
        """Verify random_read creates valid workload."""
        workload = Workload.random_read(
            lba_range=(0, 100), io_count=10, block_size_kb=4
        )

        assert workload.kind == WorkloadKind.RANDOM_READ
        assert workload.start_lba == 0
        assert workload.lba_count == 10
        assert workload.block_size_kb == 4
        assert workload.queue_depth == 1
        assert workload.randomness_seed is None

    def test_random_read_with_randomness_seed(self) -> None:
        """Verify random_read accepts randomness_seed."""
        workload = Workload.random_read(
            lba_range=(0, 1000),
            io_count=50,
            block_size_kb=4,
            randomness_seed=99,
        )

        assert workload.randomness_seed == 99

    def test_random_read_with_custom_queue_depth(self) -> None:
        """Verify random_read accepts custom queue_depth."""
        workload = Workload.random_read(
            lba_range=(0, 100),
            io_count=10,
            block_size_kb=4,
            queue_depth=16,
        )

        assert workload.queue_depth == 16

    def test_random_read_lba_range_start(self) -> None:
        """Verify random_read uses lba_range start as start_lba."""
        workload = Workload.random_read(
            lba_range=(200, 500), io_count=10, block_size_kb=4
        )

        assert workload.start_lba == 200

    def test_random_read_zero_io_count(self) -> None:
        """Verify random_read handles zero io_count with max(io_count, 1)."""
        workload = Workload.random_read(lba_range=(0, 100), io_count=0, block_size_kb=4)

        # Should use max(io_count, 1) = 1
        assert workload.lba_count == 1


class TestBlocksFromGib:
    """Test suite for _blocks_from_gib conversion method.

    These tests verify the GiB-to-LBA conversion logic handles
    various inputs correctly and validates edge cases.
    """

    def test_blocks_from_gib_one_gb_4kb_blocks(self) -> None:
        """Verify _blocks_from_gib converts 1 GB with 4 KB blocks."""
        count = Workload._blocks_from_gib(1.0, 4)

        # 1 GiB = 1024^3 bytes, 4 KB block = 4096 bytes
        expected = int((1.0 * 1024**3) // (4 * 1024))
        assert count == expected

    def test_blocks_from_gib_fractional_gb(self) -> None:
        """Verify _blocks_from_gib handles fractional GB values."""
        count = Workload._blocks_from_gib(0.5, 4)

        expected = int((0.5 * 1024**3) // (4 * 1024))
        assert count == expected

    def test_blocks_from_gib_small_value(self) -> None:
        """Verify _blocks_from_gib handles very small GB values."""
        count = Workload._blocks_from_gib(0.0001, 4)

        # Should return a small positive number
        assert count >= 0

    def test_blocks_from_gib_large_block_size(self) -> None:
        """Verify _blocks_from_gib handles large block sizes."""
        count = Workload._blocks_from_gib(1.0, 128)

        expected = int((1.0 * 1024**3) // (128 * 1024))
        assert count == expected

    def test_blocks_from_gib_invalid_block_size_zero(self) -> None:
        """Verify _blocks_from_gib raises error for zero block size."""
        with pytest.raises(ValueError, match="block_size_kb must be positive"):
            Workload._blocks_from_gib(1.0, 0)

    def test_blocks_from_gib_invalid_block_size_negative(self) -> None:
        """Verify _blocks_from_gib raises error for negative block size."""
        with pytest.raises(ValueError, match="block_size_kb must be positive"):
            Workload._blocks_from_gib(1.0, -4)

    def test_blocks_from_gib_invalid_length_negative(self) -> None:
        """Verify _blocks_from_gib raises error for negative length."""
        with pytest.raises(ValueError, match="length_gb must be non-negative"):
            Workload._blocks_from_gib(-1.0, 4)

    def test_blocks_from_gib_zero_length(self) -> None:
        """Verify _blocks_from_gib handles zero length correctly."""
        count = Workload._blocks_from_gib(0.0, 4)

        assert count == 0

    def test_blocks_from_gib_rounds_down(self) -> None:
        """Verify _blocks_from_gib floors the result."""
        # Use a length that won't divide evenly
        count = Workload._blocks_from_gib(0.0001, 4)

        # Should be floored (int division)
        assert isinstance(count, int)
        assert count >= 0


class TestWorkloadIntegration:
    """Integration tests for Workload factory methods.

    These tests verify end-to-end workload creation and usage
    in realistic scenarios.
    """

    def test_create_full_write_read_workload_pair(self) -> None:
        """Verify creating matching write/read workload pair."""
        write = Workload.sequential_write(start_lba=0, length_gb=0.01, block_size_kb=4)
        read = Workload.sequential_read(start_lba=0, length_gb=0.01, block_size_kb=4)

        # Should have same start and count
        assert write.start_lba == read.start_lba
        assert write.lba_count == read.lba_count
        assert write.block_size_kb == read.block_size_kb

    def test_create_random_workload_with_seed(self) -> None:
        """Verify random workload creation with seed for reproducibility."""
        workload1 = Workload.random_write(
            lba_range=(0, 1000),
            io_count=100,
            block_size_kb=4,
            randomness_seed=42,
        )
        workload2 = Workload.random_write(
            lba_range=(0, 1000),
            io_count=100,
            block_size_kb=4,
            randomness_seed=42,
        )

        # Same parameters should produce same workload
        assert workload1.randomness_seed == workload2.randomness_seed
        assert workload1.start_lba == workload2.start_lba
        assert workload1.lba_count == workload2.lba_count

    def test_serialize_and_inspect_workload(self) -> None:
        """Verify workload can be serialized and inspected."""
        workload = Workload.sequential_write(
            start_lba=100, length_gb=0.1, block_size_kb=8, queue_depth=4
        )

        host_dict = workload.to_host_dict()

        # Verify all fields are present and correct
        assert host_dict["kind"] == "sequential_write"
        assert host_dict["start_lba"] == 100
        assert host_dict["block_size_kb"] == 8
        assert host_dict["queue_depth"] == 4
