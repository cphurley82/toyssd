from __future__ import annotations

import time

import toyssd


def test_toyssd_basic_write_flow() -> None:
    sim = toyssd.ToySSD()

    req_id = sim.submit_write(0, 4096)
    assert isinstance(req_id, int) and req_id > 0

    completions = []
    for _ in range(20):
        completions.extend(sim.poll(4))
        if completions:
            break
        time.sleep(0.001)
    assert completions, "expected at least one completion"
    assert any(c.request_id == req_id for c in completions)

    events = sim.drain_nand_events()
    assert any(e.op == "PROGRAM" for e in events)


