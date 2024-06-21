/*
 *
 * Copyright 2021-2024 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#pragma once

#include "srsran/adt/expected.h"
#include <cstdint>
#include <cstdlib>

namespace srsran {

/// \brief 36-bit identifying an NR Cell Id as specified in subclause 9.3.1.7 of 3GPP TS 38.413.
/// \remark The leftmost (22-32) bits of the NR Cell Identity correspond to the gNB ID and remaining (4-14) bits for
/// local Cell ID.
class nr_cell_identity
{
  constexpr nr_cell_identity(uint64_t val_) : val(val_) {}

public:
  static constexpr nr_cell_identity min() { return nr_cell_identity{0x0}; }
  static constexpr nr_cell_identity max() { return nr_cell_identity{((uint64_t)1U << 36U) - 1U}; }

  static expected<nr_cell_identity> from_number(uint64_t val)
  {
    if (val > max().val) {
      return default_error_t{};
    }
    return nr_cell_identity{val};
  }

  static expected<nr_cell_identity> parse_hex(const std::string& hex_str)
  {
    const unsigned digits = nof_bits() / 4;
    if (hex_str.size() > digits) {
      return default_error_t{};
    }
    char*    p;
    uint64_t n = std::strtoul(hex_str.c_str(), &p, 16);
    if (*p != 0) {
      return default_error_t{};
    }
    return nr_cell_identity{n};
  }

  uint64_t value() const { return val; }

  static size_t nof_bits() { return 36; }

private:
  uint64_t val;
};

} // namespace srsran

namespace fmt {

template <>
struct formatter<srsran::nr_cell_identity> : formatter<uint64_t> {
  template <typename FormatContext>
  auto format(const srsran::nr_cell_identity& val, FormatContext& ctx)
  {
    return formatter<uint64_t>::format(val.value(), ctx);
  }
};

} // namespace fmt