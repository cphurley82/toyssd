// Copyright Chris Hurley
#pragma once
#include <cstdint>
#include <systemc>  // NOLINT because this should be after C++ system headers

struct PhysicalPage;

class INandInterface {
 public:
  virtual ~INandInterface() = default;
  virtual sc_core::sc_time read(const PhysicalPage&, uint8_t* dst) = 0;
  virtual sc_core::sc_time program(const PhysicalPage&, const uint8_t* src) = 0;
  virtual sc_core::sc_time erase(uint32_t die, uint32_t block) = 0;
};
