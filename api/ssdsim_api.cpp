// Copyright Chris Hurley
#include "api/ssdsim_api.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#include "sim/Config.h"

// Forward-declared adapter functions implemented in HostInterface.cpp
namespace ssdsim_internal {
int submit_cxx(void* user_tag, uint64_t lba, uint32_t size_bytes, bool is_write,
               void* buf);
int poll_cxx(int max_cpls, ssd_cpl_t* out);
int init_cxx(const char* cfg);
void shutdown_cxx();
}  // namespace ssdsim_internal

namespace {

std::optional<std::string> read_file(const char* path) {
  std::ifstream file(path);
  if (!file) {
    return std::nullopt;
  }
  std::ostringstream oss;
  oss << file.rdbuf();
  return oss.str();
}

bool parse_scalar(const std::string& text, const std::string& key,
                  double& out) {
  const std::string pattern = "\"" + key + "\"";
  auto pos = text.find(pattern);
  if (pos == std::string::npos) {
    return false;
  }
  pos = text.find(':', pos);
  if (pos == std::string::npos) {
    return false;
  }
  ++pos;
  while (pos < text.size() &&
         std::isspace(static_cast<unsigned char>(text[pos]))) {
    ++pos;
  }
  if (pos >= text.size()) {
    return false;
  }

  size_t end = pos;
  bool found_digit = false;
  while (end < text.size()) {
    const char ch = text[end];
    if (std::isdigit(static_cast<unsigned char>(ch)) || ch == '.' ||
        ch == '-' || ch == '+' || ch == 'e' || ch == 'E') {
      found_digit = true;
      ++end;
      continue;
    }
    break;
  }
  if (!found_digit) {
    return false;
  }
  const std::string token = text.substr(pos, end - pos);
  try {
    out = std::stod(token);
  } catch (...) {
    return false;
  }
  return true;
}

bool parse_uint_field(const std::string& text, const std::string& key,
                      uint32_t& out) {
  double value = 0.0;
  if (!parse_scalar(text, key, value)) {
    return false;
  }
  if (value < 0.0) {
    return false;
  }
  double intpart = 0.0;
  if (std::modf(value, &intpart) != 0.0) {
    return false;
  }
  out = static_cast<uint32_t>(intpart);
  return true;
}

bool parse_double_field(const std::string& text, const std::string& key,
                        double& out) {
  return parse_scalar(text, key, out);
}

std::optional<SimulatorConfig> load_config_from_json(const char* path,
                                                     std::string& error) {
  const auto contents = read_file(path);
  if (!contents.has_value()) {
    error = "failed to open config file: " + std::string(path ? path : "");
    return std::nullopt;
  }
  SimulatorConfig cfg{};
  const std::string& text = contents.value();
  if (!parse_uint_field(text, "dies", cfg.nand_dies) ||
      !parse_uint_field(text, "blocks_per_die", cfg.nand_blocks_per_die) ||
      !parse_uint_field(text, "pages_per_block", cfg.nand_pages_per_block) ||
      !parse_uint_field(text, "page_size_bytes", cfg.nand_page_size_bytes) ||
      !parse_double_field(text, "t_read_us", cfg.nand_t_read_us) ||
      !parse_double_field(text, "t_prog_us", cfg.nand_t_prog_us) ||
      !parse_double_field(text, "t_erase_us", cfg.nand_t_erase_us) ||
      !parse_double_field(text, "ctrl_overhead_us",
                          cfg.controller_overhead_us) ||
      !parse_uint_field(text, "rng_seed", cfg.rng_seed)) {
    error = "config file missing required fields or has invalid values";
    return std::nullopt;
  }
  if (cfg.nand_dies == 0 || cfg.nand_blocks_per_die == 0 ||
      cfg.nand_pages_per_block == 0 || cfg.nand_page_size_bytes == 0) {
    error = "config file must specify positive NAND geometry";
    return std::nullopt;
  }
  if (cfg.nand_t_read_us < 0.0 || cfg.nand_t_prog_us < 0.0 ||
      cfg.nand_t_erase_us < 0.0 || cfg.controller_overhead_us < 0.0) {
    error = "timing values must be non-negative";
    return std::nullopt;
  }
  return cfg;
}

}  // namespace

int ssdsim_init(const char* config_path) {
  SimulatorConfig cfg{};
  if (config_path != nullptr && config_path[0] != '\0') {
    std::string error;
    auto parsed = load_config_from_json(config_path, error);
    if (!parsed.has_value()) {
      std::fprintf(stderr, "ssdsim_init: %s\n", error.c_str());
      return 1;
    }
    cfg = parsed.value();
  } else {
    cfg = SimulatorConfig{};
  }
  set_simulator_config(cfg);
  return ssdsim_internal::init_cxx(config_path);
}

int ssdsim_submit(const ssd_io_t* req) {
  return ssdsim_internal::submit_cxx(req->user_tag, req->lba, req->size_bytes,
                                     req->is_write != 0, req->buf);
}

int ssdsim_poll(int max_cpls, ssd_cpl_t* out) {
  return ssdsim_internal::poll_cxx(max_cpls, out);
}

void ssdsim_shutdown(void) { ssdsim_internal::shutdown_cxx(); }
