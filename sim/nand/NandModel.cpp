// Copyright Chris Hurley
#include "sim/nand/NandModel.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace {
std::string make_key(const NandCmd::Address& addr) {
  // Compose a compact, stable key from all address fields
  // Format: ch/ce/lun/plane/block/wordline/logical_page
  return std::to_string(addr.channel) + "/" + std::to_string(addr.ce) + "/" +
         std::to_string(addr.lun) + "/" + std::to_string(addr.plane) + "/" +
         std::to_string(addr.block) + "/" + std::to_string(addr.wordline) +
         "/" + std::to_string(addr.logical_page);
}
}  // namespace

void NandModel::b_transport(tlm::tlm_generic_payload& /*gp*/,
                            sc_core::sc_time& /*delay*/) {
  // Not implemented: this simplified model uses the custom NandCmd path.
}

void NandModel::b_transport(NandCmd& cmd, sc_core::sc_time& /*delay*/) {
  const auto key = make_key(cmd.addr);

  switch (cmd.op) {
    case NandCmd::Op::PROGRAM: {
      PageStore& store = pages_[key];
      // Copy data if provided (optional)
      if (cmd.data.has_value()) {
        auto span = cmd.data.value();
        store.data.assign(span.begin(), span.end());
      } else {
        store.data.clear();
      }
      // Copy metadata (empty span => clears metadata)
      store.metadata.assign(cmd.metadata.begin(), cmd.metadata.end());
      break;
    }
    case NandCmd::Op::READ: {
      auto it_pages = pages_.find(key);
      if (it_pages != pages_.end()) {
        const PageStore& store = it_pages->second;
        if (cmd.data.has_value()) {
          auto dst = cmd.data.value();
          const size_t copy_len = std::min(dst.size(), store.data.size());
          if (copy_len > 0) {
            std::memcpy(dst.data(), store.data.data(), copy_len);
          }
          // Zero-fill any remaining bytes in the destination buffer to make
          // behavior deterministic for callers that provide larger buffers
          // than what was stored (or when no data was stored).
          if (dst.size() > copy_len) {
            std::memset(dst.data() + copy_len, 0, dst.size() - copy_len);
          }
        }
        if (!cmd.metadata.empty()) {
          const size_t meta_copy =
              std::min(cmd.metadata.size(), store.metadata.size());
          if (meta_copy > 0) {
            std::memcpy(cmd.metadata.data(), store.metadata.data(), meta_copy);
          }
          if (cmd.metadata.size() > meta_copy) {
            std::memset(cmd.metadata.data() + meta_copy, 0,
                        cmd.metadata.size() - meta_copy);
          }
        }
      } else {
        // Page not programmed/erased: return erased state (0xFF)
        if (cmd.data.has_value()) {
          auto dst = cmd.data.value();
          constexpr unsigned char kErasedByte = 0xFF;
          if (!dst.empty()) {
            std::memset(dst.data(), kErasedByte, dst.size());
          }
        }
        if (!cmd.metadata.empty()) {
          constexpr unsigned char kErasedByte = 0xFF;
          std::memset(cmd.metadata.data(), kErasedByte, cmd.metadata.size());
        }
      }
      break;
    }
    case NandCmd::Op::ERASE: {
      // Remove all pages within the same block across all pages/mlc bits
      // that match the provided channel/ce/lun/plane/block
      std::vector<std::string> to_erase;
      for (const auto& key_value : pages_) {
        // Parse key minimally: since format is fixed, we can match prefix
        // prefix = ch/ce/lun/plane/block/
        const std::string prefix = std::to_string(cmd.addr.channel) + "/" +
                                   std::to_string(cmd.addr.ce) + "/" +
                                   std::to_string(cmd.addr.lun) + "/" +
                                   std::to_string(cmd.addr.plane) + "/" +
                                   std::to_string(cmd.addr.block) + "/";
        if (key_value.first.starts_with(prefix)) {
          to_erase.push_back(key_value.first);
        }
      }
      for (const auto& key : to_erase) {
        pages_.erase(key);
      }
      break;
    }
  }
}

std::vector<NandModel::Event> NandModel::drain_events() {
  std::vector<Event> out;
  out.swap(events_);
  return out;
}

void NandModel::record_event(const NandCmd& cmd,
                             const sc_core::sc_time& delay) {
  events_.push_back(Event{
      .op = cmd.op,
      .addr = cmd.addr,
      .time_ps = (sc_core::sc_time_stamp() + delay).value(),
  });
}
