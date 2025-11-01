// Copyright Chris Hurley
#include "sim/nand/NandInterfaceImpl.h"

#include "sim/fw/FTL.h"  // for PhysicalPage definition

sc_core::sc_time NandInterfaceImpl::read(const PhysicalPage& page,
                                         uint8_t* /*dst*/) {
  NandCmd cmd;
  cmd.op = NandCmd::Op::READ;
  cmd.addr.block = page.block;
  cmd.addr.wordline = page.page;  // Map PhysicalPage.page to wordline
  // Map legacy PhysicalPage(die, block, page) to new addressing. Assume
  // single channel, single LUN/plane for now; use die as CE selector.
  cmd.addr.channel = 0;
  cmd.addr.ce = page.die;
  cmd.addr.lun = 0;
  cmd.addr.plane = 0;
  cmd.data = std::nullopt;  // Data is optional
  sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
  // Shortcut call into model (non-standard). In a full impl, use TLM GP +
  // extension.
  auto* tgt = dynamic_cast<NandModel*>(socket.get_base_port().get_interface());
  if (tgt != nullptr) {
    tgt->b_transport(cmd, delay);
  }
  return delay;
}

sc_core::sc_time NandInterfaceImpl::program(const PhysicalPage& page,
                                            const uint8_t* /*src*/) {
  NandCmd cmd;
  cmd.op = NandCmd::Op::PROGRAM;
  cmd.addr.block = page.block;
  cmd.addr.wordline = page.page;  // Map PhysicalPage.page to wordline
  cmd.addr.channel = 0;
  cmd.addr.ce = page.die;
  cmd.addr.lun = 0;
  cmd.addr.plane = 0;
  cmd.data = std::nullopt;  // Optional by design
  sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
  auto* tgt = dynamic_cast<NandModel*>(socket.get_base_port().get_interface());
  if (tgt != nullptr) {
    tgt->b_transport(cmd, delay);
  }
  return delay;
}

sc_core::sc_time NandInterfaceImpl::erase(uint32_t die, uint32_t block) {
  NandCmd cmd;
  cmd.op = NandCmd::Op::ERASE;
  cmd.addr.block = block;
  cmd.addr.wordline = 0;
  cmd.addr.channel = 0;
  cmd.addr.ce = die;
  cmd.addr.lun = 0;
  cmd.addr.plane = 0;
  cmd.data = std::nullopt;
  sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
  auto* tgt = dynamic_cast<NandModel*>(socket.get_base_port().get_interface());
  if (tgt != nullptr) {
    tgt->b_transport(cmd, delay);
  }
  return delay;
}
