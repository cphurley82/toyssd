// Copyright Chris Hurley
#include "sim/nand/NandModel.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace {
std::string make_key(const NandCmd::Address& a) {
  // Compose a compact, stable key from all address fields
  // Format: ch/ce/lun/plane/block/wordline/logical_page
  return std::to_string(a.channel) + "/" + std::to_string(a.ce) + "/" +
         std::to_string(a.lun) + "/" + std::to_string(a.plane) + "/" +
         std::to_string(a.block) + "/" + std::to_string(a.wordline) + "/" +
         std::to_string(a.logical_page);
}
}  // namespace

void NandModel::b_transport(tlm::tlm_generic_payload& /*gp*/,
                            sc_core::sc_time& /*delay*/) {
  // Not implemented: use custom NandCmd path for this simple model
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
      auto it = pages_.find(key);
      if (it != pages_.end()) {
        const PageStore& store = it->second;
        if (cmd.data.has_value()) {
          auto dst = cmd.data.value();
          const size_t n = std::min(dst.size(), store.data.size());
          if (n > 0) std::memcpy(dst.data(), store.data.data(), n);
          // If destination is larger than source, zero the remainder for
          // determinism
          if (dst.size() > n) std::memset(dst.data() + n, 0, dst.size() - n);
        }
        if (!cmd.metadata.empty()) {
          const size_t n = std::min(cmd.metadata.size(), store.metadata.size());
          if (n > 0) std::memcpy(cmd.metadata.data(), store.metadata.data(), n);
          if (cmd.metadata.size() > n)
            std::memset(cmd.metadata.data() + n, 0, cmd.metadata.size() - n);
        }
      } else {
        // Page not programmed/erased: return erased state (0xFF)
        if (cmd.data.has_value()) {
          auto dst = cmd.data.value();
          if (!dst.empty()) std::memset(dst.data(), 0xFF, dst.size());
        }
        if (!cmd.metadata.empty()) {
          std::memset(cmd.metadata.data(), 0xFF, cmd.metadata.size());
        }
      }
      break;
    }
    case NandCmd::Op::ERASE: {
      // Remove all pages within the same block across all pages/mlc bits
      // that match the provided channel/ce/lun/plane/block
      std::vector<std::string> to_erase;
      for (const auto& kv : pages_) {
        // Parse key minimally: since format is fixed, we can match prefix
        // prefix = ch/ce/lun/plane/block/
        const std::string prefix = std::to_string(cmd.addr.channel) + "/" +
                                   std::to_string(cmd.addr.ce) + "/" +
                                   std::to_string(cmd.addr.lun) + "/" +
                                   std::to_string(cmd.addr.plane) + "/" +
                                   std::to_string(cmd.addr.block) + "/";
        if (kv.first.rfind(prefix, 0) == 0) {
          to_erase.push_back(kv.first);
        }
      }
      for (const auto& k : to_erase) pages_.erase(k);
      break;
    }
  }
}
