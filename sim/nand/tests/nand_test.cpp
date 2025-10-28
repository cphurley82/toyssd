// Copyright Chris Hurley
#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <vector>

#include "sim/nand/NandModel.h"

static void fill_pattern(std::span<uint8_t> s, uint32_t val) {
  for (size_t i = 0; i < s.size(); ++i)
    s[i] = static_cast<uint8_t>((val + i) & 0xFF);
}

static uint32_t addr_signature(const NandCmd::Address& a) {
  // Simple signature combining all fields
  return (a.channel & 0xF) | ((a.ce & 0xF) << 4) | ((a.lun & 0xF) << 8) |
         ((a.plane & 0xF) << 12) | ((a.block & 0xFF) << 16) |
         ((a.wordline & 0xFF) << 24) | ((a.logical_page & 0xF) << 28);
}

TEST(NandModelAddressing, SaveAndRecallMetadataAcrossAddresses) {
  NandModel nand("nand");
  // Iterate a modest grid to keep runtime small
  for (uint32_t ch = 0; ch < 2; ++ch) {
    for (uint32_t ce = 0; ce < 2; ++ce) {
      for (uint32_t lun = 0; lun < 2; ++lun) {
        for (uint32_t plane = 0; plane < 2; ++plane) {
          for (uint32_t block = 0; block < 2; ++block) {
            for (uint32_t page = 0; page < 3; ++page) {
              for (uint32_t mlc = 0; mlc < 2; ++mlc) {
                // Program metadata unique to the address
                std::array<uint8_t, 8> meta{};
                NandCmd prog{};
                prog.op = NandCmd::Op::PROGRAM;
                prog.addr.channel = ch;
                prog.addr.ce = ce;
                prog.addr.lun = lun;
                prog.addr.plane = plane;
                prog.addr.block = block;
                prog.addr.wordline = page;
                prog.addr.logical_page = mlc;
                const uint32_t sig = addr_signature(prog.addr);
                fill_pattern(std::span<uint8_t>(meta.data(), meta.size()), sig);
                prog.data = std::nullopt;  // data optional
                prog.metadata = std::span<uint8_t>(meta.data(), meta.size());
                sc_core::sc_time d0 = sc_core::SC_ZERO_TIME;
                nand.b_transport(prog, d0);

                // Read back metadata and verify
                std::array<uint8_t, 8> meta_out{};
                NandCmd rd{};
                rd.op = NandCmd::Op::READ;
                rd.addr = prog.addr;
                rd.data = std::nullopt;  // no data buffer
                rd.metadata =
                    std::span<uint8_t>(meta_out.data(), meta_out.size());
                sc_core::sc_time d1 = sc_core::SC_ZERO_TIME;
                nand.b_transport(rd, d1);
                EXPECT_EQ(
                    0, std::memcmp(meta.data(), meta_out.data(), meta.size()));
              }
            }
          }
        }
      }
    }
  }
}

TEST(NandModelAddressing, DistinctAddressesKeepSeparateMetadata) {
  NandModel nand("nand");
  // Address A
  NandCmd::Address A{};  // all zeros
  // Address B differs in several fields
  NandCmd::Address B{};
  B.channel = 1;
  B.ce = 1;
  B.lun = 1;
  B.plane = 1;
  B.block = 1;
  B.wordline = 2;
  B.logical_page = 1;

  std::array<uint8_t, 4> metaA{1, 2, 3, 4};
  std::array<uint8_t, 4> metaB{9, 8, 7, 6};

  NandCmd pA{};
  pA.op = NandCmd::Op::PROGRAM;
  pA.addr = A;
  pA.data = std::nullopt;
  pA.metadata = metaA;
  NandCmd pB{};
  pB.op = NandCmd::Op::PROGRAM;
  pB.addr = B;
  pB.data = std::nullopt;
  pB.metadata = metaB;
  sc_core::sc_time d = sc_core::SC_ZERO_TIME;
  nand.b_transport(pA, d);
  d = sc_core::SC_ZERO_TIME;
  nand.b_transport(pB, d);

  std::array<uint8_t, 4> outA{};
  std::array<uint8_t, 4> outB{};
  NandCmd rA{};
  rA.op = NandCmd::Op::READ;
  rA.addr = A;
  rA.data = std::nullopt;
  rA.metadata = outA;
  NandCmd rB{};
  rB.op = NandCmd::Op::READ;
  rB.addr = B;
  rB.data = std::nullopt;
  rB.metadata = outB;
  d = sc_core::SC_ZERO_TIME;
  nand.b_transport(rA, d);
  d = sc_core::SC_ZERO_TIME;
  nand.b_transport(rB, d);
  EXPECT_EQ(0, std::memcmp(metaA.data(), outA.data(), outA.size()));
  EXPECT_EQ(0, std::memcmp(metaB.data(), outB.data(), outB.size()));
}

TEST(NandModelDataOptional, ProgramWithoutDataThenReadDataBuffer) {
  NandModel nand("nand");
  // Program only metadata
  NandCmd::Address addr{};
  addr.block = 3;
  addr.wordline = 4;
  addr.logical_page = 1;
  std::array<uint8_t, 6> meta{};
  for (size_t i = 0; i < meta.size(); ++i)
    meta[i] = static_cast<uint8_t>(i + 1);

  NandCmd prog{};
  prog.op = NandCmd::Op::PROGRAM;
  prog.addr = addr;
  prog.data = std::nullopt;
  prog.metadata = meta;
  sc_core::sc_time d0 = sc_core::SC_ZERO_TIME;
  nand.b_transport(prog, d0);

  // Read with data + metadata buffers: data should zero-fill (not stored),
  // metadata should match
  std::array<uint8_t, 16> data_out{};
  data_out.fill(0xAA);
  std::array<uint8_t, 6> meta_out{};
  meta_out.fill(0);
  NandCmd rd{};
  rd.op = NandCmd::Op::READ;
  rd.addr = addr;
  rd.data = std::span<uint8_t>(data_out.data(), data_out.size());
  rd.metadata = meta_out;
  sc_core::sc_time d1 = sc_core::SC_ZERO_TIME;
  nand.b_transport(rd, d1);

  // Data zeroed
  for (auto b : data_out) EXPECT_EQ(b, 0);
  // Metadata matches
  EXPECT_EQ(0, std::memcmp(meta.data(), meta_out.data(), meta.size()));
}

TEST(NandModelErase, EraseAnyPageClearsWholeBlock) {
  NandModel nand("nand");
  // Use same channel/ce/lun/plane, different pages in the same block
  NandCmd::Address base{};
  base.channel = 0;
  base.ce = 0;
  base.lun = 0;
  base.plane = 0;
  base.block = 7;

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
  pA.metadata = metaA;
  sc_core::sc_time d = sc_core::SC_ZERO_TIME;
  nand.b_transport(pA, d);
  NandCmd pB{};
  pB.op = NandCmd::Op::PROGRAM;
  pB.addr = base;
  pB.addr.wordline = 2;
  pB.addr.logical_page = 1;
  pB.data = std::nullopt;
  pB.metadata = metaB;
  d = sc_core::SC_ZERO_TIME;
  nand.b_transport(pB, d);

  // Program a page in a different block (should remain after erase)
  NandCmd::Address other = base;
  other.block = 8;
  other.wordline = 1;
  other.logical_page = 0;
  NandCmd pOther{};
  pOther.op = NandCmd::Op::PROGRAM;
  pOther.addr = other;
  pOther.data = std::nullopt;
  pOther.metadata = metaOther;
  d = sc_core::SC_ZERO_TIME;
  nand.b_transport(pOther, d);

  // Erase the block by issuing ERASE for any page in that block
  NandCmd er{};
  er.op = NandCmd::Op::ERASE;
  er.addr = base;
  er.addr.wordline = 99;  // wordline ignored for erase scope
  d = sc_core::SC_ZERO_TIME;
  nand.b_transport(er, d);

  // Read back pages from erased block -> metadata should be 0xFF (erased state)
  std::array<uint8_t, 4> outA{};
  outA.fill(0xFF);
  std::array<uint8_t, 4> outB{};
  outB.fill(0xFF);
  NandCmd rA{};
  rA.op = NandCmd::Op::READ;
  rA.addr = base;
  rA.addr.wordline = 1;
  rA.addr.logical_page = 0;
  rA.data = std::nullopt;
  rA.metadata = outA;
  d = sc_core::SC_ZERO_TIME;
  nand.b_transport(rA, d);
  NandCmd rB{};
  rB.op = NandCmd::Op::READ;
  rB.addr = base;
  rB.addr.wordline = 2;
  rB.addr.logical_page = 1;
  rB.data = std::nullopt;
  rB.metadata = outB;
  d = sc_core::SC_ZERO_TIME;
  nand.b_transport(rB, d);
  for (auto b : outA) EXPECT_EQ(0xFF, b);
  for (auto b : outB) EXPECT_EQ(0xFF, b);

  // Page from a different block should remain intact
  std::array<uint8_t, 4> outOther{};
  outOther.fill(0);
  NandCmd rOther{};
  rOther.op = NandCmd::Op::READ;
  rOther.addr = other;
  rOther.data = std::nullopt;
  rOther.metadata = outOther;
  d = sc_core::SC_ZERO_TIME;
  nand.b_transport(rOther, d);
  EXPECT_EQ(0, std::memcmp(metaOther.data(), outOther.data(), outOther.size()));
}
