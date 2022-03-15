#ifndef SRSGNB_UNITTEST_PHY_CHANNEL_PROCESSORS_PDCCH_MODULATOR_TEST_DATA_H_
#define SRSGNB_UNITTEST_PHY_CHANNEL_PROCESSORS_PDCCH_MODULATOR_TEST_DATA_H_

#include "../../resource_grid_test_doubles.h"
#include "srsgnb/phy/upper/channel_processors/pdcch_modulator.h"
#include <vector>

namespace srsgnb {

struct test_case_t {
  pdcch_modulator::config_t               config;
  std::vector<uint8_t>                    data;
  std::vector<resource_grid_spy::entry_t> symbols;
};

static const std::vector<test_case_t> pdcch_modulator_test_data = {
    // No data to test.
};

} // namespace srsgnb

#endif // SRSGNB_UNITTEST_PHY_CHANNEL_PROCESSORS_PDCCH_MODULATOR_TEST_DATA_H_
