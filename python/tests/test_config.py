"""Comprehensive unit tests for the config module.

Test Coverage Goals:
- Validate NandGeometry property calculations and edge cases
- Validate SimConfig validation logic for all configurable parameters
- Ensure error messages are clear and actionable
- Test both valid and invalid configuration scenarios
"""

import pytest

from toyssd import NandGeometry, SimConfig


class TestNandGeometry:
    """Test suite for NandGeometry configuration and property calculations.

    These tests ensure that the geometry calculations correctly compute
    derived properties like total capacity, block sizes, and page counts.
    Edge cases verify that the geometry handles various realistic scenarios.
    """

    def test_default_geometry(self) -> None:
        """Verify default geometry values are sensible and consistent."""
        geometry = NandGeometry()
        assert geometry.capacity_gb == 1
        assert geometry.dies == 4
        assert geometry.blocks_per_die == 1024
        assert geometry.pages_per_block == 256
        assert geometry.page_size_bytes == 16_384
        assert geometry.oob_size_bytes == 1024
        assert geometry.planes_per_die == 2

    def test_total_blocks_calculation(self) -> None:
        """Verify total_blocks correctly multiplies dies and blocks_per_die."""
        geometry = NandGeometry(dies=2, blocks_per_die=100)
        assert geometry.total_blocks == 200

    def test_total_blocks_single_die(self) -> None:
        """Verify total_blocks works correctly with a single die."""
        geometry = NandGeometry(dies=1, blocks_per_die=50)
        assert geometry.total_blocks == 50

    def test_pages_per_die_total_calculation(self) -> None:
        """Verify pages_per_die_total correctly multiplies blocks and pages."""
        geometry = NandGeometry(blocks_per_die=100, pages_per_block=256)
        assert geometry.pages_per_die_total == 25600

    def test_pages_per_die_total_small_geometry(self) -> None:
        """Verify pages_per_die_total with minimal geometry values."""
        geometry = NandGeometry(blocks_per_die=10, pages_per_block=16)
        assert geometry.pages_per_die_total == 160

    def test_block_size_bytes_calculation(self) -> None:
        """Verify block_size_bytes correctly multiplies pages and page size."""
        geometry = NandGeometry(pages_per_block=256, page_size_bytes=16_384)
        expected = 256 * 16_384
        assert geometry.block_size_bytes == expected

    def test_block_size_bytes_custom_values(self) -> None:
        """Verify block_size_bytes with custom page configurations."""
        geometry = NandGeometry(pages_per_block=128, page_size_bytes=8192)
        expected = 128 * 8192
        assert geometry.block_size_bytes == expected

    def test_total_capacity_bytes_calculation(self) -> None:
        """Verify total_capacity_bytes accounts for full geometry."""
        geometry = NandGeometry(
            dies=2,
            blocks_per_die=100,
            pages_per_block=64,
            page_size_bytes=4096,
        )
        expected = 2 * 100 * 64 * 4096
        assert geometry.total_capacity_bytes == expected

    def test_total_capacity_bytes_default_geometry(self) -> None:
        """Verify total_capacity_bytes for default geometry."""
        geometry = NandGeometry()
        expected = 4 * 1024 * 256 * 16_384
        assert geometry.total_capacity_bytes == expected

    def test_geometry_is_frozen(self) -> None:
        """Verify NandGeometry is immutable (frozen dataclass)."""
        geometry = NandGeometry()
        with pytest.raises(AttributeError):
            geometry.dies = 8  # type: ignore

    def test_custom_geometry_all_parameters(self) -> None:
        """Verify custom geometry with all parameters specified."""
        geometry = NandGeometry(
            capacity_gb=8,
            dies=8,
            blocks_per_die=2048,
            pages_per_block=512,
            page_size_bytes=32_768,
            oob_size_bytes=2048,
            planes_per_die=4,
        )
        assert geometry.capacity_gb == 8
        assert geometry.dies == 8
        assert geometry.blocks_per_die == 2048
        assert geometry.pages_per_block == 512
        assert geometry.page_size_bytes == 32_768
        assert geometry.oob_size_bytes == 2048
        assert geometry.planes_per_die == 4


class TestSimConfig:
    """Test suite for SimConfig validation and initialization.

    These tests ensure that SimConfig correctly validates user input,
    provides clear error messages for invalid configurations, and
    properly initializes with valid parameters.
    """

    def test_default_config(self) -> None:
        """Verify default configuration values are valid and functional."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry)
        assert config.nand_geometry == geometry
        assert config.backend == "python"
        assert config.enable_visualization is True
        assert config.log_level == "INFO"
        assert config.event_buffer_capacity == 1024
        assert config.time_step_ms == 1
        assert config.host_queue_depth == 1
        assert config.seed is None

    def test_custom_config_all_parameters(self) -> None:
        """Verify custom configuration with all parameters specified."""
        geometry = NandGeometry(capacity_gb=2)
        config = SimConfig(
            nand_geometry=geometry,
            backend="python",
            enable_visualization=False,
            log_level="DEBUG",
            event_buffer_capacity=2048,
            time_step_ms=10,
            host_queue_depth=32,
            seed=42,
        )
        assert config.nand_geometry.capacity_gb == 2
        assert config.backend == "python"
        assert config.enable_visualization is False
        assert config.log_level == "DEBUG"
        assert config.event_buffer_capacity == 2048
        assert config.time_step_ms == 10
        assert config.host_queue_depth == 32
        assert config.seed == 42

    def test_invalid_host_queue_depth_zero(self) -> None:
        """Verify that host_queue_depth=0 raises a clear error."""
        geometry = NandGeometry()
        with pytest.raises(ValueError, match="host_queue_depth must be >= 1"):
            SimConfig(nand_geometry=geometry, host_queue_depth=0)

    def test_invalid_host_queue_depth_negative(self) -> None:
        """Verify that negative host_queue_depth raises a clear error."""
        geometry = NandGeometry()
        with pytest.raises(ValueError, match="host_queue_depth must be >= 1"):
            SimConfig(nand_geometry=geometry, host_queue_depth=-5)

    def test_invalid_time_step_ms_zero(self) -> None:
        """Verify that time_step_ms=0 raises a clear error."""
        geometry = NandGeometry()
        with pytest.raises(ValueError, match="time_step_ms must be positive"):
            SimConfig(nand_geometry=geometry, time_step_ms=0)

    def test_invalid_time_step_ms_negative(self) -> None:
        """Verify that negative time_step_ms raises a clear error."""
        geometry = NandGeometry()
        with pytest.raises(ValueError, match="time_step_ms must be positive"):
            SimConfig(nand_geometry=geometry, time_step_ms=-10)

    def test_invalid_event_buffer_capacity_zero(self) -> None:
        """Verify that event_buffer_capacity=0 raises a clear error."""
        geometry = NandGeometry()
        with pytest.raises(ValueError, match="event_buffer_capacity must be positive"):
            SimConfig(nand_geometry=geometry, event_buffer_capacity=0)

    def test_invalid_event_buffer_capacity_negative(self) -> None:
        """Verify that negative event_buffer_capacity raises a clear error."""
        geometry = NandGeometry()
        with pytest.raises(ValueError, match="event_buffer_capacity must be positive"):
            SimConfig(nand_geometry=geometry, event_buffer_capacity=-100)

    def test_invalid_log_level_lowercase(self) -> None:
        """Verify that invalid log level (even if proper case) raises error."""
        geometry = NandGeometry()
        with pytest.raises(ValueError, match="log_level must be one of"):
            SimConfig(nand_geometry=geometry, log_level="invalid")

    def test_invalid_log_level_random_string(self) -> None:
        """Verify that random log level string raises a clear error."""
        geometry = NandGeometry()
        with pytest.raises(ValueError, match="log_level must be one of.*got random"):
            SimConfig(nand_geometry=geometry, log_level="random")

    def test_valid_log_levels(self) -> None:
        """Verify all valid log levels are accepted."""
        geometry = NandGeometry()
        for level in ["DEBUG", "INFO", "WARNING", "ERROR"]:
            config = SimConfig(nand_geometry=geometry, log_level=level)
            assert config.log_level == level

    def test_log_level_case_insensitive_validation(self) -> None:
        """Verify log level validation handles mixed case correctly."""
        geometry = NandGeometry()
        # These should work because validation uses .upper()
        for level in ["debug", "info", "warning", "error"]:
            config = SimConfig(nand_geometry=geometry, log_level=level)
            assert config.log_level.upper() in {"DEBUG", "INFO", "WARNING", "ERROR"}

    def test_invalid_backend_empty_string(self) -> None:
        """Verify that empty backend string raises a clear error."""
        geometry = NandGeometry()
        with pytest.raises(ValueError, match="backend must be one of"):
            SimConfig(nand_geometry=geometry, backend="")

    def test_invalid_backend_random_string(self) -> None:
        """Verify that invalid backend raises a clear error."""
        geometry = NandGeometry()
        with pytest.raises(ValueError, match="backend must be one of.*got invalid"):
            SimConfig(nand_geometry=geometry, backend="invalid")

    def test_valid_backends(self) -> None:
        """Verify both valid backends are accepted."""
        geometry = NandGeometry()
        for backend in ["python", "systemc"]:
            # Note: systemc backend will fail at ToySSD init, not config validation
            config = SimConfig(nand_geometry=geometry, backend=backend)
            assert config.backend == backend

    def test_backend_case_insensitive_validation(self) -> None:
        """Verify backend validation handles mixed case correctly."""
        geometry = NandGeometry()
        # Test that validation uses .lower()
        config = SimConfig(nand_geometry=geometry, backend="PYTHON")
        assert config.backend.lower() == "python"

    def test_optional_seed_none(self) -> None:
        """Verify that seed can be None (default behavior)."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, seed=None)
        assert config.seed is None

    def test_optional_seed_with_value(self) -> None:
        """Verify that seed can be set to a specific integer."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry, seed=12345)
        assert config.seed == 12345

    def test_config_is_mutable(self) -> None:
        """Verify SimConfig is mutable (not frozen) for runtime adjustments."""
        geometry = NandGeometry()
        config = SimConfig(nand_geometry=geometry)
        # Should be able to modify since frozen=False by default
        config.log_level = "DEBUG"
        assert config.log_level == "DEBUG"
