#include "sched_ssb.h"

#define NOF_SLOTS_PER_SUBFRAME 1
#define NOF_SSB_OFDM_SYMBOLS 4
#define NOF_SSB_PRBS 20
#define CUTOFF_FREQ_ARFCN_CASE_A_B_C 600000
#define CUTOFF_FREQ_ARFCN_CASE_C_UNPAIRED 376000

namespace srsgnb {

static void fill_ssb_parameters(uint32_t    offset_to_point_A,
                                uint8_t     ofdm_sym_idx,
                                uint8_t     ssb_idx,
                                uint8_t     in_burst_bitmap,
                                ssb_list_t& ssb_list)
{
  ssb_t ssb_msg = {};
  // Determines whether this is the first transmission of the burst. To do this, mask the "8-ssb_idx" in_burst_bitmap's
  // from the right. If the mask operation is equal to the in_burst_bitmap, then the ssb_idx is the first transmission
  // of the burst. E.g.:   in_burst_bitmap          =      [ 0 1 0 1 1 1 0 0 ]
  //         mask_1 (for ssb_idx = 1) =      [ 0 1 1 1 1 1 1 1 ]
  //         mask_4 (for ssb_idx = 4) =      [ 0 0 0 0 1 1 1 1 ]
  // in_burst_bitmap & mask_1 =  [ 0 1 0 1 1 1 0 0 ]  =>  ssb_idx = 1 is the first transmission of the burst
  // in_burst_bitmap & mask_1 =  [ 0 0 0 0 1 1 0 0 ]  =>  ssb_idx = 4 is  NOT the first transmission of the burs
  bool is_first_transmission = (in_burst_bitmap & (0b11111111 >> ssb_idx)) == in_burst_bitmap;
  ssb_msg.tx_mode            = is_first_transmission ? ssb_transmission : ssb_repetition;
  ssb_msg.ssb_info.ssb_idx   = ssb_idx;
  ssb_msg.ssb_info.ofdm_symbols.set(ofdm_sym_idx, ofdm_sym_idx + NOF_SSB_OFDM_SYMBOLS);
  // The value below is common for all SSB, regardless of the different parameters or cases.
  // NOTE: It assumes the reference grid is that of the SSB
  ssb_msg.ssb_info.prb_alloc.set(offset_to_point_A, offset_to_point_A + NOF_SSB_PRBS);
  // NOTE: until we understand whether the time and frequency allocation refer to the common grid or SSB grid, we
  // leave this extra line (commented) below, which will be used if the reference is the common grid;
  // ssb_msg.ssb_info.prb_alloc.set(offset_to_point_A, NOF_SSB_PRBS + floor(k_ssb % MAX_K_SSB));
  ssb_list.push_back(ssb_msg);
}

/// Perform allocation for case A and C (both paired and unpaired spectrum)
static void ssb_alloc_case_A_C(uint32_t          freq_arfcn,
                               uint32_t          freq_arfcn_cut_off,
                               const slot_point& sl_point_mod,
                               uint32_t          offset_to_point_A,
                               uint8_t           in_burst_bitmap,
                               ssb_list_t&       ssb_list)
{
  /// The OFDM symbols allocations for Case A and case C are identical; the only difference is the cutoff frequency,
  /// which is 3GHz for case A and C paired, but 1.88GHz for case C unpaired
  if (freq_arfcn <= freq_arfcn_cut_off and sl_point_mod > 1) {
    return;
  }

  /// In case A and C, the candidate OFDM symbols where to allocate the SSB within its period are indexed as follows:
  /// n = 2, 8, 16, 22  for frequencies <= cutoff frequency
  /// n = 2, 8, 16, 22, 30, 36, 44, 50  for frequencies > cutoff frequency
  /// Cutoff frequency is: 3GHz for Case A and Case C paired, 1.88GHz for Case C unpaired
  /// Slot 0 has OFDM symbols 2,8; Slot n has symbols {2,8} + 14 * n;
  /// TS 38.213 Section 4
  if (sl_point_mod == 0 or sl_point_mod == 1 or sl_point_mod == 2 or sl_point_mod == 3) {
    /// The SSB mask is used to choose only the SSB indices corresponding to the current slot.
    uint8_t                ssb_idx_mask = 0b10000000 >> (sl_point_mod * 2);
    std::array<uint8_t, 2> ofdm_symbols = {2, 8};

    for (uint8_t n = 0; n < ofdm_symbols.size(); n++) {
      if (in_burst_bitmap & ssb_idx_mask) {
        srsran_assert(n < sizeof(ofdm_symbols), "SSB index exceeding OFDM symbols array size");
        uint8_t ssb_idx = n + sl_point_mod * 2;
        fill_ssb_parameters(offset_to_point_A,
                            ofdm_symbols[n] + sl_point_mod * NOF_ODFM_SYMB_PER_SLOT,
                            ssb_idx,
                            in_burst_bitmap,
                            ssb_list);
      }
      ssb_idx_mask = ssb_idx_mask >> 1;
    }
  }
}

static void ssb_alloc_case_B(uint32_t          freq_arfcn,
                             const slot_point& sl_point_mod,
                             uint32_t          offset_to_point_A,
                             uint8_t           in_burst_bitmap,
                             ssb_list_t&       ssb_list)
{
  if (freq_arfcn <= CUTOFF_FREQ_ARFCN_CASE_A_B_C and sl_point_mod > 1) {
    return;
  }

  /// In case B, the candidate OFDM symbols where to allocate the SSB within its period are indexed as follows:
  /// n = 4, 8, 16, 20  for frequencies <= cutoff frequency
  /// n = 4, 8, 16, 20, 32, 36, 44, 48  for frequencies > cutoff frequency
  /// Cutoff frequency is: 3GHz for Case B
  /// TS 38.213 Section 4.1

  /// Slot 0 has OFDM symbols 4,8; Slot 2 has symbols 32, 36;
  if (sl_point_mod == 0 or sl_point_mod == 2) {
    /// The SSB mask is used to choose only the SSB indices corresponding to the current slot.
    uint8_t                ssb_idx_mask = 0b10000000 >> (sl_point_mod * 2);
    std::array<uint8_t, 2> ofdm_symbols = {4, 8};

    for (uint8_t n = 0; n < ofdm_symbols.size(); n++) {
      if (in_burst_bitmap & ssb_idx_mask) {
        srsran_assert(n < sizeof(ofdm_symbols), "SSB index exceeding OFDM symbols array size");
        uint8_t ssb_idx = n + sl_point_mod * 2;
        fill_ssb_parameters(offset_to_point_A,
                            ofdm_symbols[n] + sl_point_mod * NOF_ODFM_SYMB_PER_SLOT,
                            ssb_idx,
                            in_burst_bitmap,
                            ssb_list);
      }
      ssb_idx_mask = ssb_idx_mask >> 1;
    }
  }

  /// Slot 0 has OFDM symbols 4,8; Slot 1 has symbols 16, 20; Slot 3 has symbols 32, 36; Slot 3 has symbols 44, 48
  if (sl_point_mod == 1 or sl_point_mod == 3) {
    /// The SSB mask is used to choose only the SSB indices corresponding to the current slot.
    uint8_t                ssb_idx_mask = 0b00100000 >> ((sl_point_mod - 1) * 2);
    std::array<uint8_t, 2> ofdm_symbols = {16, 20};

    for (uint8_t n = 0; n < ofdm_symbols.size(); n++) {
      if (in_burst_bitmap & ssb_idx_mask) {
        srsran_assert(n < sizeof(ofdm_symbols), "SSB index exceeding OFDM symbols array size");
        uint8_t ssb_idx = n + sl_point_mod * 2;
        fill_ssb_parameters(offset_to_point_A,
                            ofdm_symbols[n] + (sl_point_mod - 1) * NOF_ODFM_SYMB_PER_SLOT,
                            ssb_idx,
                            in_burst_bitmap,
                            ssb_list);
      }
      ssb_idx_mask = ssb_idx_mask >> 1;
    }
  }
}

void sched_ssb(const slot_point& sl_point,
               uint16_t          ssb_periodicity,
               uint32_t          offset_to_point_A,
               uint32_t          freq_arfcn,
               uint8_t           in_burst_bitmap,
               ssb_alloc_case    ssb_case,
               ssb_list_t&       ssb_list)
{
  if (ssb_list.full()) {
    srslog::fetch_basic_logger("MAC-NR").error("SCHED: Failed to allocate SSB");
    return;
  }

  // If the periodicity is 0, it means that the parameter was not passed by the upper layers.
  // In that case, we use default value of 5ms (see Clause 4.1, TS 38.213)
  if (ssb_periodicity == 0) {
    ssb_periodicity = DEFAULT_SSB_PERIODICITY;
  }

  srsran_assert(freq_arfcn < 875000, "Frenquencies in the range FR2 not supported");

  // Perform mod operation of slot index by ssb_periodicity;
  // "ssb_periodicity * nof_slots_per_subframe" gives the number of slots in 1 ssb_periodicity time interval
  uint32_t sl_point_mod = sl_point % (ssb_periodicity * NOF_SLOTS_PER_SUBFRAME);

  // Select SSB case with reference to TS 38.213, Section 4.1
  switch (ssb_case) {
    case ssb_case_A:
    case ssb_case_C_paired:
      ssb_alloc_case_A_C(
          freq_arfcn, CUTOFF_FREQ_ARFCN_CASE_A_B_C, sl_point_mod, offset_to_point_A, in_burst_bitmap, ssb_list);
      break;
    case ssb_case_B:
      ssb_alloc_case_B(freq_arfcn, sl_point_mod, offset_to_point_A, in_burst_bitmap, ssb_list);
      break;
    case ssb_case_C_unpaired:
      ssb_alloc_case_A_C(
          freq_arfcn, CUTOFF_FREQ_ARFCN_CASE_C_UNPAIRED, sl_point_mod, offset_to_point_A, in_burst_bitmap, ssb_list);
      break;
    default:
      srsran_assert(ssb_case < ssb_case_invalid, "Only SSB case A, B and C are currently supported");
      break;
  }
}

} // namespace srsgnb
