// Copyright Chris Hurley
#include "sim/nand/NandInterfaceImpl.h"

#include "sim/Config.h"
#include "sim/fw/FTL.h"  // for PhysicalPage definition

void NandInterfaceImpl::attach(NandModel* model) { model_ = model; }

namespace {
inline sc_core::sc_time make_delay(double micros) {
  if (micros <= 0.0) {
    return sc_core::SC_ZERO_TIME;
  }
  return sc_core::sc_time(micros, sc_core::SC_US);
}

inline NandModel* resolve_model(
    NandModel* attached,
    tlm_utils::simple_initiator_socket<NandInterfaceImpl>& socket) {
  if (attached != nullptr) {
    return attached;
  }
  auto* iface = socket.get_base_port().get_interface();
  return dynamic_cast<NandModel*>(iface);
}
}  // namespace

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
  const auto& cfg = get_simulator_config();
  sc_core::sc_time delay = make_delay(cfg.nand_t_read_us);
  auto* tgt = resolve_model(model_, socket);
  if (tgt != nullptr) {
    tgt->b_transport(cmd, delay);
    tgt->record_event(cmd, delay);
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
  const auto& cfg = get_simulator_config();
  sc_core::sc_time delay = make_delay(cfg.nand_t_prog_us);
  auto* tgt = resolve_model(model_, socket);
  if (tgt != nullptr) {
    tgt->b_transport(cmd, delay);
    tgt->record_event(cmd, delay);
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
  const auto& cfg = get_simulator_config();
  sc_core::sc_time delay = make_delay(cfg.nand_t_erase_us);
  auto* tgt = resolve_model(model_, socket);
  if (tgt != nullptr) {
    tgt->b_transport(cmd, delay);
    tgt->record_event(cmd, delay);
  }
  return delay;
}
