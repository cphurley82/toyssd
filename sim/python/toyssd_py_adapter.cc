// Copyright 2025 Chris Hurley

#include "sim/python/toyssd_py_adapter.h"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace toyssd::python {

namespace {
constexpr uint8_t kDefaultWritePattern = 0x5A;
}

ToyssdPyAdapter::ToyssdPyAdapter() { reset(); }

ToyssdPyAdapter::~ToyssdPyAdapter() = default;

void ToyssdPyAdapter::initialize_top() {
  top_ = std::make_unique<Top>("toyssd_top");
  host_ = &top_->host;
  sc_core::sc_start(sc_core::SC_ZERO_TIME);
}

void ToyssdPyAdapter::reset() {
  top_.reset();
  tokens_.clear();
  payloads_.clear();
  host_ = nullptr;
  next_id_ = 0;
  simctx_ = std::make_unique<sc_core::sc_simcontext>();
  initialize_top();
}

uint64_t ToyssdPyAdapter::submit(uint64_t lba, uint32_t size_bytes,
                                 bool is_write) {
  if (host_ == nullptr) {
    throw std::runtime_error("ToyssdPyAdapter not initialized");
  }
  auto token = std::make_unique<RequestToken>();
  token->id = ++next_id_;
  token->is_write = is_write;

  IORequest req{
      .user_tag = token.get(),
      .lba = lba,
      .size_bytes = size_bytes,
      .is_write = is_write,
      .buf = nullptr,
      .submit_ts = sc_core::sc_time_stamp(),
  };

  if (is_write && size_bytes > 0) {
    auto data = std::vector<uint8_t>(size_bytes);
    for (uint32_t i = 0; i < size_bytes; ++i) {
      data[i] = static_cast<uint8_t>((token->id + i) & 0xFF);
    }
    if (std::all_of(data.begin(), data.end(),
                    [](uint8_t v) { return v == 0; })) {
      std::fill(data.begin(), data.end(), kDefaultWritePattern);
    }
    req.buf = data.data();
    payloads_.emplace(token->id, std::move(data));
  }

  if (host_->submit(req) != 0) {
    payloads_.erase(token->id);
    throw std::runtime_error("HostInterface busy: submit failed");
  }

  tokens_.emplace(token->id, std::move(token));
  return next_id_;
}

uint64_t ToyssdPyAdapter::submit_write(uint64_t lba, uint32_t size_bytes) {
  return submit(lba, size_bytes, true);
}

uint64_t ToyssdPyAdapter::submit_read(uint64_t lba, uint32_t size_bytes) {
  return submit(lba, size_bytes, false);
}

std::vector<PyCompletion> ToyssdPyAdapter::poll(int max_completions) {
  if (max_completions <= 0 || host_ == nullptr) {
    return {};
  }
  std::vector<ssd_cpl_t> raw(static_cast<size_t>(max_completions));
  int total = 0;
  constexpr int kMaxIters = 100;
  constexpr int kStepUs = 10;
  for (int iter = 0; iter < kMaxIters && total < max_completions; ++iter) {
    const int produced =
        host_->poll(max_completions - total, raw.data() + total);
    if (produced > 0) {
      total += produced;
      if (total >= max_completions) {
        break;
      }
    }
    sc_core::sc_start(sc_core::sc_time(kStepUs, sc_core::SC_US));
  }
  std::vector<PyCompletion> out;
  out.reserve(static_cast<size_t>(total));
  for (int i = 0; i < total; ++i) {
    const auto tag = static_cast<RequestToken*>(raw[i].user_tag);
    uint64_t id = tag ? tag->id : 0;
    out.push_back(PyCompletion{
        .request_id = id,
        .status = raw[i].status,
        .completion_ns = raw[i].ns,
    });
    tokens_.erase(id);
    payloads_.erase(id);
  }
  return out;
}

std::vector<NandModel::Event> ToyssdPyAdapter::drain_nand_events() {
  if (!top_) {
    return {};
  }
  return top_->nand.drain_events();
}

void ToyssdPyAdapter::run_for_ns(uint64_t ns) {
  if (ns == 0) {
    return;
  }
  sc_core::sc_start(sc_core::sc_time(static_cast<double>(ns), sc_core::SC_NS));
}

void ToyssdPyAdapter::run_for_us(uint64_t us) { run_for_ns(us * 1000); }

void ToyssdPyAdapter::run_for_ms(uint64_t ms) { run_for_ns(ms * 1'000'000); }

uint64_t ToyssdPyAdapter::time_ps() const {
  return sc_core::sc_time_stamp().value();
}

}  // namespace toyssd::python
