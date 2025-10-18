#include "Firmware.h"

#include "../nand/NandInterface.h"

using namespace sc_core;

void Firmware::run() {
  while (true) {
    IORequest* r = in.read();
    // Minimal FTL
    auto ppa = r->is_write ? ftl.map_write(r->lba) : ftl.map_read(r->lba);

    sc_time delay = SC_ZERO_TIME;
    if (nand_if) {
      if (r->is_write)
        delay = nand_if->program(ppa, r->buf);
      else
        delay = nand_if->read(ppa, r->buf);
    }

    wait(delay + ctrl_overhead());
    out.write(Completion{r->user_tag, 0, sc_time_stamp()});
    delete r;
  }
}
