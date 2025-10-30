// Copyright Chris Hurley
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

#include "sim/nand/NandModel.h"

namespace {

void fill_pattern(std::span<uint8_t> s, uint32_t val) {
  for (size_t i = 0; i < s.size(); ++i) {
    s[i] = static_cast<uint8_t>((val + i) & 0xFF);
  }
}

uint32_t addr_signature(const NandCmd::Address& a) {
  // Simple signature combining all fields
  return (a.channel & 0xF) | ((a.ce & 0xF) << 4) | ((a.lun & 0xF) << 8) |
         ((a.plane & 0xF) << 12) | ((a.block & 0xFF) << 16) |
         ((a.wordline & 0xFF) << 24) | ((a.logical_page & 0xF) << 28);
}

// Optional small helpers to reduce boilerplate when constructing commands.
NandCmd MakeProgram(const NandCmd::Address& addr,
                    std::optional<std::span<uint8_t>> data,
                    std::span<uint8_t> metadata) {
  NandCmd cmd{};
  cmd.op = NandCmd::Op::PROGRAM;
  cmd.addr = addr;
  cmd.data = data;
  cmd.metadata = metadata;
  return cmd;
}

NandCmd MakeRead(const NandCmd::Address& addr,
                 std::optional<std::span<uint8_t>> data,
                 std::span<uint8_t> metadata) {
  NandCmd cmd{};
  cmd.op = NandCmd::Op::READ;
  cmd.addr = addr;
  cmd.data = data;
  cmd.metadata = metadata;
  return cmd;
}

}  // namespace

class NandModelTest : public ::testing::Test {
 protected:
  NandModel nand{"nand"};
};

TEST_F(NandModelTest, SaveAndRecallMetadataAcrossAddresses) {
  // Iterate a modest grid to keep runtime small
  for (uint32_t ch = 0; ch < 2; ++ch) {
    for (uint32_t ce = 0; ce < 2; ++ce) {
      for (uint32_t lun = 0; lun < 2; ++lun) {
        for (uint32_t plane = 0; plane < 2; ++plane) {
          for (uint32_t block = 0; block < 2; ++block) {
            for (uint32_t page = 0; page < 3; ++page) {
              for (uint32_t logical_page = 0; logical_page < 2;
                   ++logical_page) {
                // Program metadata unique to the address
                std::array<uint8_t, 8> meta{};
                auto a = NandCmd::Address{}
                             .with_channel(ch)
                             .with_ce(ce)
                             .with_lun(lun)
                             .with_plane(plane)
                             .with_block(block)
                             .with_wordline(page)
                             .with_logical_page(logical_page);
                const uint32_t sig = addr_signature(a);
                fill_pattern(std::span<uint8_t>(meta), sig);
                {
                  SCOPED_TRACE(::testing::Message()
                               << "addr_sig=" << std::hex << sig);
                  auto delay0 = sc_core::SC_ZERO_TIME;
                  auto prog =
                      MakeProgram(a, std::nullopt, std::span<uint8_t>(meta));
                  nand.b_transport(prog, delay0);

                  // Read back metadata and verify
                  std::array<uint8_t, 8> meta_out{};
                  auto delay1 = sc_core::SC_ZERO_TIME;
                  auto rd =
                      MakeRead(a, std::nullopt, std::span<uint8_t>(meta_out));
                  nand.b_transport(rd, delay1);
                  EXPECT_EQ(meta, meta_out);
                }
              }
            }
          }
        }
      }
    }
  }
}

TEST_F(NandModelTest, DistinctAddressesKeepSeparateMetadata) {
  // Address A
  const NandCmd::Address A{};  // all zeros
  // Address B differs in several fields
  auto B = NandCmd::Address{}
               .with_channel(1)
               .with_ce(1)
               .with_lun(1)
               .with_plane(1)
               .with_block(1)
               .with_wordline(2)
               .with_logical_page(1);

  std::array<uint8_t, 4> metaA{1, 2, 3, 4};
  std::array<uint8_t, 4> metaB{9, 8, 7, 6};

  auto pA = MakeProgram(A, std::nullopt, std::span<uint8_t>(metaA));
  auto pB = MakeProgram(B, std::nullopt, std::span<uint8_t>(metaB));
  auto delay = sc_core::SC_ZERO_TIME;
  nand.b_transport(pA, delay);
  delay = sc_core::SC_ZERO_TIME;
  nand.b_transport(pB, delay);

  std::array<uint8_t, 4> outA{};
  std::array<uint8_t, 4> outB{};
  auto rA = MakeRead(A, std::nullopt, std::span<uint8_t>(outA));
  auto rB = MakeRead(B, std::nullopt, std::span<uint8_t>(outB));
  delay = sc_core::SC_ZERO_TIME;
  nand.b_transport(rA, delay);
  delay = sc_core::SC_ZERO_TIME;
  nand.b_transport(rB, delay);
  EXPECT_EQ(metaA, outA);
  EXPECT_EQ(metaB, outB);
}

TEST_F(NandModelTest, ProgramWithoutDataThenReadDataBuffer) {
  // Program only metadata
  auto addr =
      NandCmd::Address{}.with_block(3).with_wordline(4).with_logical_page(1);
  std::array<uint8_t, 6> meta{};
  for (size_t i = 0; i < meta.size(); ++i)
    meta[i] = static_cast<uint8_t>(i + 1);

  auto prog = MakeProgram(addr, std::nullopt, std::span<uint8_t>(meta));
  auto delay0 = sc_core::SC_ZERO_TIME;
  nand.b_transport(prog, delay0);

  // Read with data + metadata buffers: data should zero-fill (not stored),
  // metadata should match
  std::array<uint8_t, 16> data_out{};
  data_out.fill(0xAA);
  std::array<uint8_t, 6> meta_out{};
  meta_out.fill(0);
  auto rd = MakeRead(addr, std::span<uint8_t>(data_out),
                     std::span<uint8_t>(meta_out));
  auto delay1 = sc_core::SC_ZERO_TIME;
  nand.b_transport(rd, delay1);

  // Data zeroed
  EXPECT_TRUE(std::all_of(data_out.begin(), data_out.end(),
                          [](auto b) { return b == 0; }));
  // Metadata matches
  EXPECT_EQ(meta, meta_out);
}

TEST_F(NandModelTest, EraseAnyPageClearsWholeBlock) {
  // Use same channel/ce/lun/plane, different pages in the same block
  auto base = NandCmd::Address{}.with_block(7);

  std::array<uint8_t, 4> metaA{0xAA, 0xBB, 0xCC, 0xDD};
  std::array<uint8_t, 4> metaB{0x11, 0x22, 0x33, 0x44};
  std::array<uint8_t, 4> metaOther{0xDE, 0xAD, 0xBE, 0xEF};

  // Program two pages in block 7
  NandCmd pA{};
  pA.op = NandCmd::Op::PROGRAM;
  pA.addr = base;
  pA.addr.wordline = 1;
  pA.addr.logical_page = 0;
  pA.data = std::nullopt;
  pA.metadata = std::span<uint8_t>(metaA);
  auto delay = sc_core::SC_ZERO_TIME;
  nand.b_transport(pA, delay);
  NandCmd pB{};
  pB.op = NandCmd::Op::PROGRAM;
  pB.addr = base;
  pB.addr.wordline = 2;
  pB.addr.logical_page = 1;
  pB.data = std::nullopt;
  pB.metadata = std::span<uint8_t>(metaB);
  delay = sc_core::SC_ZERO_TIME;
  nand.b_transport(pB, delay);

  // Program a page in a different block (should remain after erase)
  auto other = base.with_block(8).with_wordline(1).with_logical_page(0);
  NandCmd pOther{};
  pOther.op = NandCmd::Op::PROGRAM;
  pOther.addr = other;
  pOther.data = std::nullopt;
  pOther.metadata = std::span<uint8_t>(metaOther);
  delay = sc_core::SC_ZERO_TIME;
  nand.b_transport(pOther, delay);

  // Erase the block by issuing ERASE for any page in that block
  NandCmd er{};
  er.op = NandCmd::Op::ERASE;
  er.addr = base;
  er.addr.wordline = 99;  // wordline ignored for erase scope
  delay = sc_core::SC_ZERO_TIME;
  nand.b_transport(er, delay);

  // Read back pages from erased block -> metadata should be 0xFF (erased state)
  std::array<uint8_t, 4> outA{};
  outA.fill(0xAA);  // non-0xFF prefill to ensure READ writes metadata
  std::array<uint8_t, 4> outB{};
  outB.fill(0xAA);  // non-0xFF prefill to ensure READ writes metadata
  NandCmd rA{};
  rA.op = NandCmd::Op::READ;
  rA.addr = base;
  rA.addr.wordline = 1;
  rA.addr.logical_page = 0;
  rA.data = std::nullopt;
  rA.metadata = std::span<uint8_t>(outA);
  delay = sc_core::SC_ZERO_TIME;
  nand.b_transport(rA, delay);
  NandCmd rB{};
  rB.op = NandCmd::Op::READ;
  rB.addr = base;
  rB.addr.wordline = 2;
  rB.addr.logical_page = 1;
  rB.data = std::nullopt;
  rB.metadata = std::span<uint8_t>(outB);
  delay = sc_core::SC_ZERO_TIME;
  nand.b_transport(rB, delay);
  for (auto b : outA) EXPECT_EQ(0xFF, b);
  for (auto b : outB) EXPECT_EQ(0xFF, b);

  // Page from a different block should remain intact
  std::array<uint8_t, 4> outOther{};
  outOther.fill(0);
  NandCmd rOther{};
  rOther.op = NandCmd::Op::READ;
  rOther.addr = other;
  rOther.data = std::nullopt;
  rOther.metadata = std::span<uint8_t>(outOther);
  delay = sc_core::SC_ZERO_TIME;
  nand.b_transport(rOther, delay);
  EXPECT_EQ(metaOther, outOther);
}
