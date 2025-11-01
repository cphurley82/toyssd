// Copyright Chris Hurley
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "systemc"
#include "tlm"
#include "tlm_utils/simple_target_socket.h"

struct NandCmd {
  enum class Op { READ, PROGRAM, ERASE } op;

  struct Address {
    uint32_t channel{0};   // Channel (shared bus)
    uint32_t ce{0};        // Chip Enable selects a package/die on the channel
    uint32_t lun{0};       // LUN within the die
    uint32_t plane{0};     // Plane within the LUN
    uint32_t block{0};     // Block within the plane
    uint32_t wordline{0};  // Physical row within the block (physical page)
    uint32_t logical_page{0};  // Logical page/program step within the wordline

    // Convenience: chainable "named parameter" style setters that return a
    // modified copy. Keep the type an aggregate by not declaring any
    // user-defined constructors so C++20 designated initializers remain
    // available as well.
    [[nodiscard]] constexpr Address with_channel(uint32_t v) const {
      Address a = *this;
      a.channel = v;
      return a;
    }
    [[nodiscard]] constexpr Address with_ce(uint32_t v) const {
      Address a = *this;
      a.ce = v;
      return a;
    }
    [[nodiscard]] constexpr Address with_lun(uint32_t v) const {
      Address a = *this;
      a.lun = v;
      return a;
    }
    [[nodiscard]] constexpr Address with_plane(uint32_t v) const {
      Address a = *this;
      a.plane = v;
      return a;
    }
    [[nodiscard]] constexpr Address with_block(uint32_t v) const {
      Address a = *this;
      a.block = v;
      return a;
    }
    [[nodiscard]] constexpr Address with_wordline(uint32_t v) const {
      Address a = *this;
      a.wordline = v;
      return a;
    }
    [[nodiscard]] constexpr Address with_logical_page(uint32_t v) const {
      Address a = *this;
      a.logical_page = v;
      return a;
    }

    // Convenience: static factory with defaults for all fields. This keeps
    // designated-init viable while offering a compact call form.
    static constexpr Address make(uint32_t channel = 0, uint32_t ce = 0,
                                  uint32_t lun = 0, uint32_t plane = 0,
                                  uint32_t block = 0, uint32_t wordline = 0,
                                  uint32_t logical_page = 0) {
      return Address{channel, ce, lun, plane, block, wordline, logical_page};
    }
  } addr;

  // Optional data payload (e.g., page data). For READ, simulators may ignore
  // or fill this buffer. For PROGRAM, simulators may read from this buffer.
  // Data is optional by design.
  std::optional<std::span<uint8_t>> data{};

  // Separate metadata buffer (OOB/spare area). Empty span means no metadata.
  std::span<uint8_t> metadata{};
};

struct NandModel : sc_core::sc_module {
  // TLM-2.0 target socket for standard transactions
  tlm_utils::simple_target_socket<NandModel> socket;

  SC_CTOR(NandModel) {
    socket.register_b_transport(this, &NandModel::b_transport);
  }

  // Standard TLM b_transport (required by simple_target_socket)
  void b_transport(tlm::tlm_generic_payload& gp, sc_core::sc_time& delay);

  // Overload for custom payload shortcut (not standard TLM; placeholder)
  void b_transport(NandCmd& cmd, sc_core::sc_time& delay);

 private:
  struct PageStore {
    std::vector<uint8_t> data;
    std::vector<uint8_t> metadata;
  };
  // Keyed by a composite string of the full address
  // (channel/ce/lun/plane/block/wordline/logical_page)
  std::unordered_map<std::string, PageStore> pages_;
};
