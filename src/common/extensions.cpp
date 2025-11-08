// Copyright toyssd contributors
// SPDX-License-Identifier: MIT
//
// File: extensions.cpp
// Brief: Implements tlm_extension clone/copy hooks for NVMe and NAND command
//        extensions.

#include "toyssd/extensions.hpp"

namespace toyssd {

tlm::tlm_extension_base* NvmeCommandExtension::clone() const {
  return new NvmeCommandExtension(*this);
}

void NvmeCommandExtension::copy_from(const tlm_extension_base& ext) {
  const auto& other = static_cast<const NvmeCommandExtension&>(ext);
  *this = other;
}

tlm::tlm_extension_base* NandCommandExtension::clone() const {
  return new NandCommandExtension(*this);
}

void NandCommandExtension::copy_from(const tlm_extension_base& ext) {
  const auto& other = static_cast<const NandCommandExtension&>(ext);
  *this = other;
}

}  // namespace toyssd
