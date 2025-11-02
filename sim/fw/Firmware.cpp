// Copyright Chris Hurley
#include "sim/fw/Firmware.h"

#include "sim/nand/NandInterface.h"

// Avoid using-directives; use explicit sc_core:: qualifiers.

void Firmware::run() {
  while (true) {
    IORequest* req = in.read();
    // Minimal FTL
    const auto ppa =
        req->is_write ? ftl.map_write(req->lba) : ftl.map_read(req->lba);

    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    if (nand_if != nullptr) {
      if (req->is_write) {
        delay = nand_if->program(ppa, req->buf);
      } else {
        delay = nand_if->read(ppa, req->buf);
      }
    }

    wait(delay + ctrl_overhead());
    const auto completion_time = sc_core::sc_time_stamp();
    const auto latency = completion_time - req->submit_ts;
    out.write(Completion{
        .user_tag = req->user_tag,
        .status = 0,
        .complete_ts = completion_time,
        .latency = latency,
    });
    delete req;
  }
}
