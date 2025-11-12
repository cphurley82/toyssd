"""Comprehensive unit tests for the visualization module.

Test Coverage Goals:
- Test Visualization initialization and state management
- Test event processing and state updates
- Test rendering with various event types and states
- Test snapshot saving and history tracking
- Test glyph mapping for all event types
- Test edge cases and error conditions
"""

import tempfile
from pathlib import Path

import pytest

from toyssd import NandGeometry
from toyssd.visualization import BlockState, Visualization


class TestBlockState:
    """Test suite for BlockState data class.
    
    These tests verify the BlockState dataclass correctly stores
    LBA and state information for rendering.
    """

    def test_block_state_initialization(self) -> None:
        """Verify BlockState initializes with LBA and state."""
        state = BlockState(lba=42, state="write")
        assert state.lba == 42
        assert state.state == "write"

    def test_block_state_different_states(self) -> None:
        """Verify BlockState can represent different event types."""
        write_state = BlockState(lba=0, state="write")
        read_state = BlockState(lba=1, state="read")
        erase_state = BlockState(lba=2, state="erase")
        
        assert write_state.state == "write"
        assert read_state.state == "read"
        assert erase_state.state == "erase"


class TestVisualizationInitialization:
    """Test suite for Visualization initialization.
    
    These tests verify that Visualization properly initializes with
    geometry and starts with clean state.
    """

    def test_init_with_default_geometry(self) -> None:
        """Verify Visualization initializes with default geometry."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        assert viz._geometry == geometry
        assert len(viz._history) == 0
        assert len(viz._state) == 0

    def test_init_with_custom_geometry(self) -> None:
        """Verify Visualization initializes with custom geometry."""
        geometry = NandGeometry(dies=8, blocks_per_die=2048)
        viz = Visualization(geometry)
        assert viz._geometry.dies == 8
        assert viz._geometry.blocks_per_die == 2048


class TestVisualizationUpdate:
    """Test suite for event processing via update method.
    
    These tests verify that the visualization correctly processes
    various event types and maintains state.
    """

    def test_update_with_empty_events(self) -> None:
        """Verify update handles empty event list gracefully."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        viz.update([])
        
        assert len(viz._history) == 0
        assert len(viz._state) == 0

    def test_update_with_write_event(self) -> None:
        """Verify update processes write events correctly."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        events = [{"type": "write", "lba": 10}]
        viz.update(events)
        
        assert 10 in viz._state
        assert viz._state[10].state == "write"
        assert "WRITE lba=10" in viz._history

    def test_update_with_read_event(self) -> None:
        """Verify update processes read events correctly."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        events = [{"type": "read", "lba": 20}]
        viz.update(events)
        
        assert 20 in viz._state
        assert viz._state[20].state == "read"
        assert "READ lba=20" in viz._history

    def test_update_with_erase_event(self) -> None:
        """Verify update processes erase events correctly."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        events = [{"type": "erase", "lba": 30}]
        viz.update(events)
        
        assert 30 in viz._state
        assert viz._state[30].state == "erase"
        assert "ERASE lba=30" in viz._history

    def test_update_ignores_non_relevant_events(self) -> None:
        """Verify update ignores events that aren't write/read/erase."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        events = [
            {"type": "queue", "workload": "sequential_write"},
            {"type": "idle", "duration_ms": 100},
        ]
        viz.update(events)
        
        # These events should be ignored
        assert len(viz._state) == 0
        assert len(viz._history) == 0

    def test_update_with_multiple_events(self) -> None:
        """Verify update processes multiple events in order."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        events = [
            {"type": "write", "lba": 0},
            {"type": "write", "lba": 1},
            {"type": "read", "lba": 0},
        ]
        viz.update(events)
        
        assert len(viz._state) == 2
        assert viz._state[0].state == "read"  # Latest state
        assert viz._state[1].state == "write"
        assert len(viz._history) == 3

    def test_update_with_mixed_event_types(self) -> None:
        """Verify update handles mixed relevant and irrelevant events."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        events = [
            {"type": "queue", "workload": "test"},  # Ignored
            {"type": "write", "lba": 5},
            {"type": "idle", "duration_ms": 50},  # Ignored
            {"type": "read", "lba": 5},
        ]
        viz.update(events)
        
        assert len(viz._state) == 1
        assert viz._state[5].state == "read"
        assert len(viz._history) == 2

    def test_update_overwrites_previous_state(self) -> None:
        """Verify update overwrites previous state for same LBA."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        # First update
        viz.update([{"type": "write", "lba": 42}])
        assert viz._state[42].state == "write"
        
        # Second update overwrites
        viz.update([{"type": "read", "lba": 42}])
        assert viz._state[42].state == "read"


class TestVisualizationRender:
    """Test suite for render method.
    
    These tests verify that the visualization correctly renders
    the current state as text.
    """

    def test_render_empty_state(self) -> None:
        """Verify render returns placeholder when no activity."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        output = viz.render()
        
        assert output == "<no activity>"

    def test_render_single_write(self) -> None:
        """Verify render displays a single write event."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        viz.update([{"type": "write", "lba": 0}])
        output = viz.render()
        
        assert "000000:[#]" in output

    def test_render_single_read(self) -> None:
        """Verify render displays a single read event."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        viz.update([{"type": "read", "lba": 1}])
        output = viz.render()
        
        assert "000001:[=]" in output

    def test_render_single_erase(self) -> None:
        """Verify render displays a single erase event."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        viz.update([{"type": "erase", "lba": 2}])
        output = viz.render()
        
        assert "000002:[ ]" in output

    def test_render_multiple_lbas_sorted(self) -> None:
        """Verify render displays multiple LBAs in sorted order."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        viz.update([
            {"type": "write", "lba": 10},
            {"type": "read", "lba": 5},
            {"type": "write", "lba": 20},
        ])
        output = viz.render()
        
        lines = output.strip().split("\n")
        assert len(lines) == 3
        assert "000005:[=]" in lines[0]
        assert "000010:[#]" in lines[1]
        assert "000020:[#]" in lines[2]

    def test_render_formats_lba_with_padding(self) -> None:
        """Verify render formats LBA with proper zero-padding."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        viz.update([{"type": "write", "lba": 42}])
        output = viz.render()
        
        assert "000042:[#]" in output


class TestVisualizationSaveSnapshot:
    """Test suite for save_snapshot method.
    
    These tests verify that snapshots are correctly saved to disk.
    """

    def test_save_snapshot_creates_file(self) -> None:
        """Verify save_snapshot creates a file with rendered content."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        viz.update([{"type": "write", "lba": 0}])
        
        with tempfile.TemporaryDirectory() as tmpdir:
            path = Path(tmpdir) / "snapshot.txt"
            viz.save_snapshot(path)
            
            assert path.exists()
            content = path.read_text()
            assert "000000:[#]" in content

    def test_save_snapshot_with_string_path(self) -> None:
        """Verify save_snapshot accepts string path."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        viz.update([{"type": "read", "lba": 5}])
        
        with tempfile.TemporaryDirectory() as tmpdir:
            path = str(Path(tmpdir) / "snapshot.txt")
            viz.save_snapshot(path)
            
            assert Path(path).exists()
            content = Path(path).read_text()
            assert "000005:[=]" in content

    def test_save_snapshot_empty_state(self) -> None:
        """Verify save_snapshot saves empty state placeholder."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        with tempfile.TemporaryDirectory() as tmpdir:
            path = Path(tmpdir) / "empty.txt"
            viz.save_snapshot(path)
            
            assert path.exists()
            content = path.read_text()
            assert content == "<no activity>"

    def test_save_snapshot_overwrites_existing_file(self) -> None:
        """Verify save_snapshot overwrites existing files."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        with tempfile.TemporaryDirectory() as tmpdir:
            path = Path(tmpdir) / "snapshot.txt"
            
            # Create initial file
            path.write_text("old content")
            
            # Save snapshot
            viz.update([{"type": "write", "lba": 99}])
            viz.save_snapshot(path)
            
            content = path.read_text()
            assert "000099:[#]" in content
            assert "old content" not in content


class TestVisualizationHistory:
    """Test suite for history tracking.
    
    These tests verify that event history is properly maintained.
    """

    def test_history_empty_initially(self) -> None:
        """Verify history is empty on initialization."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        history = viz.history()
        
        assert history == []

    def test_history_tracks_write_events(self) -> None:
        """Verify history tracks write events."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        viz.update([{"type": "write", "lba": 10}])
        history = viz.history()
        
        assert len(history) == 1
        assert "WRITE lba=10" in history[0]

    def test_history_tracks_read_events(self) -> None:
        """Verify history tracks read events."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        viz.update([{"type": "read", "lba": 20}])
        history = viz.history()
        
        assert len(history) == 1
        assert "READ lba=20" in history[0]

    def test_history_tracks_erase_events(self) -> None:
        """Verify history tracks erase events."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        viz.update([{"type": "erase", "lba": 30}])
        history = viz.history()
        
        assert len(history) == 1
        assert "ERASE lba=30" in history[0]

    def test_history_accumulates_events(self) -> None:
        """Verify history accumulates all events in order."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        viz.update([{"type": "write", "lba": 0}])
        viz.update([{"type": "read", "lba": 0}])
        viz.update([{"type": "erase", "lba": 0}])
        
        history = viz.history()
        
        assert len(history) == 3
        assert "WRITE lba=0" in history[0]
        assert "READ lba=0" in history[1]
        assert "ERASE lba=0" in history[2]

    def test_history_returns_copy(self) -> None:
        """Verify history returns a copy, not the internal list."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        viz.update([{"type": "write", "lba": 5}])
        history1 = viz.history()
        
        # Modify the returned list
        history1.append("modified")
        
        # Original history should be unchanged
        history2 = viz.history()
        assert len(history2) == 1
        assert "modified" not in history2


class TestVisualizationGlyphMapping:
    """Test suite for glyph mapping logic.
    
    These tests verify that _glyph_for_state correctly maps
    state strings to visual glyphs.
    """

    def test_glyph_for_write_state(self) -> None:
        """Verify write state maps to [#] glyph."""
        glyph = Visualization._glyph_for_state("write")
        assert glyph == "[#]"

    def test_glyph_for_read_state(self) -> None:
        """Verify read state maps to [=] glyph."""
        glyph = Visualization._glyph_for_state("read")
        assert glyph == "[=]"

    def test_glyph_for_erase_state(self) -> None:
        """Verify erase state maps to [ ] glyph."""
        glyph = Visualization._glyph_for_state("erase")
        assert glyph == "[ ]"

    def test_glyph_for_unknown_state(self) -> None:
        """Verify unknown state maps to [?] glyph."""
        glyph = Visualization._glyph_for_state("unknown")
        assert glyph == "[?]"

    def test_glyph_for_empty_state(self) -> None:
        """Verify empty string state maps to [?] glyph."""
        glyph = Visualization._glyph_for_state("")
        assert glyph == "[?]"


class TestVisualizationIntegration:
    """Integration tests for Visualization with realistic scenarios.
    
    These tests verify end-to-end visualization behavior with
    realistic event sequences.
    """

    def test_write_read_sequence(self) -> None:
        """Verify visualization handles write-then-read sequence."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        # Simulate write
        viz.update([{"type": "write", "lba": 100}])
        
        # Simulate read
        viz.update([{"type": "read", "lba": 100}])
        
        # State should show read (latest)
        assert viz._state[100].state == "read"
        
        # History should have both
        history = viz.history()
        assert len(history) == 2

    def test_multiple_lbas_sequential(self) -> None:
        """Verify visualization handles sequential write pattern."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        events = [{"type": "write", "lba": i} for i in range(10)]
        viz.update(events)
        
        assert len(viz._state) == 10
        assert len(viz.history()) == 10
        
        output = viz.render()
        lines = output.strip().split("\n")
        assert len(lines) == 10

    def test_sparse_lba_pattern(self) -> None:
        """Verify visualization handles sparse LBA access pattern."""
        geometry = NandGeometry()
        viz = Visualization(geometry)
        
        # Sparse access: 0, 100, 1000
        viz.update([
            {"type": "write", "lba": 0},
            {"type": "write", "lba": 100},
            {"type": "write", "lba": 1000},
        ])
        
        output = viz.render()
        assert "000000:[#]" in output
        assert "000100:[#]" in output
        assert "001000:[#]" in output
